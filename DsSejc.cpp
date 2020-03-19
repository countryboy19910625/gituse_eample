#include "StdAfx.h"
#include <boost/foreach.hpp>
#include "DsSejc.h"
#include "common\StringUtils.h"
#include "libjson\Source\NumberToString.h"
#include "JsonTools.h"
#include <boost/foreach.hpp>

const int MAX_RETRY = 4;  //任务异常最多重新下发4次

CDsSejc::~CDsSejc()
{
}

void CDsSejc::DoTask()
{
	//先截图，后校验税额
	LOG_FUNC_INFO("申报状态为已申报，直接进行税额校验");

	TString strTaxName = IdToTaxName(m_spCurrentTaskGroup->m_strGroupId);   //这里打开的是申报页面的报表名称
	if (strTaxName.empty())
	{
		NotifyDone(ExcuteFailed, TEXT("不支持的任务ID：【") + m_spCurrentTaskGroup->m_strGroupId + TEXT("】"));
		return;
	}

	//1、截图
	if (!GetHZPicBase64(NULL, m_SnapshotData))
	{
		NotifyDone(ExcuteFailed, "截图失败！");
		return;
	}
	LOG_FUNC_INFO("申报纳税页面申报完成截图成功！");

	//2、判定是否需要回执
	TString rtnJson;
	IS_N_RET(LoadJsFile(EST_STATE_SBNS, IDR_JS_DS_CHECK_DECL), sLoadJsErrMsg);
	if (!InvokeScriptString(EST_STATE_SBNS, rtnJson, TEXT("SBNS_check_tax_decl"), strTaxName))
	{
		m_OpenWebStateID = -1;
		AddImage("未找到", GetWebState(EST_STATE_SBNS)->GetHwnd());
		NotifyDone(ExcuteFailedTaxation, rtnJson);
		return;
	}
	TString strStatus = CJsonTools::GetJsonVal(rtnJson, TEXT("status"));
//	m_iCode = boost::contains(strStatus, "已申报(已导入)") ? DeclaredDone : NeedRecvReceipt;
	m_iCode = DeclaredDone;

	//3、获取税额信息
	LOG_FUNC_INFO("打开税表【%s】获取税额信息", strTaxName.c_str());
	if (!InvokeScriptString(EST_STATE_SBNS, rtnJson, TEXT("yzf_click_link_text"), TEXT("a"), strTaxName))
	{
		RetryOpertaion("申报纳税页面点击申报表失败");
		return;
	}
}


void CDsSejc::TimerHandle(UINT_PTR nIDEvent)
{
	TString strTaxName = IdToTaxName(m_spCurrentTaskGroup->m_strGroupId);   //这里打开的是申报页面的报表名称
	m_pTaskCtrl->KillTimer(nIDEvent);
	switch (nIDEvent)
	{
	case ID_TIMER_TAX_DECL_LIST_COMPLETE:
	{
		if (!m_bflag)
		{
			//事实上只有印花税才有中间页面
			LOG_FUNC_INFO("当前实际处于【%s】中间页面", strTaxName.c_str());
			TString strItem = m_spCurrentTaskGroup->m_strSsqs + "~" + m_spCurrentTaskGroup->m_strSsqz;
			TString js = "$('a:contains(" + strItem + ")')[0].click()";
			if (!m_spCurrentState->ExcuteScript(js))
			{
				if (!m_spCurrentState->ExcuteScript(js))
				{
					NotifyDone(ExcuteFailed, "任务重试到达上限！点击中间页面列表失败,请检查是否是日期不对！");
					return;
				}
			}
		}
	}
	break;

	case ID_TIMER_TAX_DECL_TABLE_OPEN:
	{
		LOG_FUNC_INFO("当前实际处于【%s】导航页面", strTaxName.c_str());
		TString strMsg;
		//这里打开的应该导航页面上的申报表
		int iCount = 0;
		while (iCount++ < 3)
		{
			if (IsExistKeyWord(_T("申报"), _T("a")))
			{
				break;
			}
			MsgSleep(3000);
		}
		if (iCount >= 3)
		{
			LOG_FUNC_INFO("【%s】导航页面加载失败！", strTaxName.c_str());
			RetryOpertaion("导航页面加载失败");
			return;
		}

		if (!m_spCurrentState->ExcuteScript("$(\"a:contains(申报)\")[0].click()"))
		{
			if (!m_spCurrentState->ExcuteScript("$(\"a:contains(申报)\")[0].click()"))
			{
				RetryOpertaion("导航页面点击【申报】失败");
				return;
			}
		}
	}
	break;

	case ID_TIMER_TAX_DECL_TABLE_COMPLETE:
	{
		LOG_FUNC_INFO("当前实际处于【%s】打开页面", strTaxName.c_str());

		//添加一个税表加载完全的条件 
		MsgSleep(3000);
		int iCount = 0;
		while (iCount++ < 3)
		{
			if (IsExistKeyWord(_T("全申报"), _T("a")))
			{
				break;
			}
			MsgSleep(3000);
		}
		if (iCount >= 3)
		{
			LOG_FUNC_INFO("【%s】填写页面加载失败！", strTaxName.c_str());
			RetryOpertaion("填写页面加载失败");
			return;
		}

		IS_N_RET(LoadJsFile(EID_STATE_TAX_OPENED_TABLE, IDR_JS_DS_TAX_DECL), sLoadJsErrMsg);
		SeJc();

	}
	break;

	default:
		return __super::TimerHandle(nIDEvent);
		break;
	}
}



HRESULT CDsSejc::OnAjaxComplete(int iStatus, const TString& strUrl, const TString& strResponseData,
	const TString& strData)
{
	if (boost::contains(strUrl, TEXT("etax.zhejiang.chinatax.gov.cn/zjgfzjdzswjsbweb/pages/sb/nssb/sb_nssb.html")))
	{
		//DoNothing but cannot  be lacked//申报纳税页面,网址和打开页面网址有部分重合
	}
	else if (boost::contains(strUrl, TEXT("pages/sb/nssb/sb_dcsb.html")))  //进入了中间页面
	{
		m_pTaskCtrl->SetTimer(ID_TIMER_TAX_DECL_LIST_COMPLETE, SLEEP_AJAX_COMPLETE_TIME);
	}
	else if (boost::contains(strUrl, TEXT("sb/nssb/bdlb.html")))  //进入了导航说明页面
	{
		m_bflag = true;
		m_pTaskCtrl->SetTimer(ID_TIMER_TAX_DECL_TABLE_OPEN, SLEEP_AJAX_COMPLETE_TIME);
	}
	else if (boost::contains(strUrl, TEXT("/zjgfcsszjdzswjsbweb/pages/sb"))
		|| boost::contains(strUrl, TEXT("zjgfzjdzswjsbweb/pages/sb"))
		|| boost::contains(strUrl, TEXT("dzswjsbwebxyw/pages/sb")))  //进入打开页面
	{
		m_pTaskCtrl->SetTimer(ID_TIMER_TAX_DECL_TABLE_COMPLETE, SLEEP_AJAX_COMPLETE_TIME);
	}

	return S_OK;
}


void CDsSejc::SeJc()
{
	LOG_FUNC_INFO("【获取并且比对税额】");
	TString strMsg;
	TString strSz = TEXT("");
	TString strWebValue = "";

	if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_CJRJYBZJ_SBB")) { strSz = TEXT("残保金"); }
	if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_TY_SBB")) { strSz = TEXT("通用申报"); }
	if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_SBDWJFR_SBB")) { strSz = TEXT("社保"); }

	if ( m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_CJRJYBZJ_SBB")
		|| m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_TY_SBB"))
	{
		const TString strTaskId = boost::algorithm::erase_all_copy(m_taskId, ID_SEJC);
		const auto& taskItem = m_pTaskCtrl->GetTaskItemById(strTaskId);
		BOOST_FOREACH(auto& cell, taskItem->m_vecVerifyCells)
		{
			TString yspzmc = cell.m_strAttrId;     //应税凭证名称
			TString column = cell.m_mapAttrs["col"];
			TString TaxName = cell.m_strTaxName;

			//根据信息找到对应单元格
			if (cell.m_mapAttrs.find("se") != cell.m_mapAttrs.end() && "true" == cell.m_mapAttrs["se"])
			{
				std::vector<TString> vecParam;
				vecParam.push_back(yspzmc);

				//印花税的列当完成申报后由浮动行变成固定行
				if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_YHS_SBB"))
				{
					int col = atoi(column.c_str()) - 1;
					CStringUtils::Format(column, "%d", col);
				}
				vecParam.push_back(column);
				vecParam.push_back(strSz);

				if (!ExcuteJsFunc(EID_STATE_TAX_OPENED_TABLE, TEXT("YzfGetCellValue"), vecParam, strWebValue))
				{
					CStringUtils::Format(strMsg, "单元格【%s】取值错误:【%s】", TaxName.c_str(), strWebValue.c_str());
					LOG_FUNC_ERR("%s", strMsg.c_str());
					NotifyDone(ExcuteFailed, strMsg);
					return;
				}
				break;
			}
		}
		if (strWebValue == "")
		{
			LOG_FUNC_INFO("获取税局税额值为空，请检查！");
			NotifyDone(ExcuteFailedUser, "获取税局税额值为空，请检查！", false, true);
			return;
		}

		ComPareSe(strWebValue);
	}
	
	else if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_YHS_SBB"))
	{
		if (!GetWebState(EID_STATE_TAX_OPENED_TABLE)->InvokeScriptFunc(strWebValue, "yzf_check_loaded_elements", "span", "bqybtse_hj"))
		{
			m_iCheckCount++;
			if (m_iCheckCount >= 5)
			{
				LOG_FUNC_INFO("没有检测到关键元素，无法获取税局税额");
				NotifyDone(ExcuteFailed, _T("没有检测到关键元素，无法获取税局税额"));
				return;
			}
			GetWebState(EID_STATE_TAX_OPENED_TABLE)->Refresh();
			return;
		}
		ComPareSe(strWebValue);

	}
	else if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_SBDWJFR_SBB"))
	{
		const TString strTaskId = boost::algorithm::erase_all_copy(m_taskId, ID_SEJC);
		const auto& taskItem = m_pTaskCtrl->GetTaskItemById(strTaskId);
		BOOST_FOREACH(auto& cell, taskItem->m_vecWriteCells)
		{
			if (cell.m_mapAttrs.find("se") != cell.m_mapAttrs.end() && "true" == cell.m_mapAttrs["se"])
			{
				TString &strId = cell.m_strAttrName;
				TString fz, zspm, zszm, strCol;
				fz = "合计";
				strCol = strId;
				TString rtnJson;
				if (!ExecScriptString(EID_STATE_TAX_OPENED_TABLE, rtnJson, TEXT("BlankCell_Value_Get_Shbx"), fz, zspm, zszm, strCol))
				{
					return;
				}
				strWebValue = CJsonTools::GetJsonVal(rtnJson, "value");
				if (strWebValue == "")
				{
					LOG_FUNC_INFO("获取税局税额值为空，请检查！");
					NotifyDone(ExcuteFailedUser, "获取税局税额值为空，请检查！", false, true);
					return;
				}
				break;
			}
		}
		ComPareSe(strWebValue);
	}
	else if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_CJJYDFJYFJ_SBB"))
	{
		MsgSleep(2000);
		if (!GetWebState(EID_STATE_TAX_OPENED_TABLE)->InvokeScriptFunc(strMsg, "yzf_check_loaded_elements", "span", "bqybtse_hj"))
		{
			m_iCheckCount++;
			if (m_iCheckCount >= 5)
			{
				LOG_FUNC_INFO("没有检测到关键元素，无法获取税局税额");
				NotifyDone(ExcuteFailed, _T("没有检测到关键元素，无法获取税局税额"));
				return;
			}
			GetWebState(EID_STATE_TAX_OPENED_TABLE)->Refresh();
			return;
		}

		TString strSe = "";
		auto iter = m_spCurrentTaskGroup->m_listTaskItems.begin();
		for (iter; iter != m_spCurrentTaskGroup->m_listTaskItems.end(); iter++)
		{
			if ((*iter)->m_strTaskId == _T("ZHEJIANGDS_CJJYDFJYFJ_SBB"))
			{
				for (auto iterVerify = (*iter)->m_vecVerifyCells.begin(); iterVerify != (*iter)->m_vecVerifyCells.end(); iterVerify++)
				{
					if ((*iterVerify).m_strAttrId == "bqybtse_hj")
					{
						strSe = (*iterVerify).m_strValue;
						break;
					}
				}
			}
		}
		if (strSe.empty())
		{
			LOG_FUNC_INFO("已申报但是未检查到任务报文中的税额属性，无法进行申报税额校验，请检查报表！");
			NotifyDone(ExcuteFailedUser, "已申报但是未检查到任务报文中的税额属性，无法进行申报税额校验，请检查报表！");
			return;
		}

		LOG_FUNC_INFO("已申报校验:获取税局应补退税额值");
		GetInnerText(EID_STATE_TAX_OPENED_TABLE, "#bqybtse_hj", strWebValue);
		if (strWebValue == "")
		{
			LOG_FUNC_INFO("获取税局税额值为空，请检查！");
			NotifyDone(ExcuteFailedUser, "获取税局税额值为空，请检查！", false, true);
			return;
		}
		ComPareSe(strWebValue, strSe);
	}
}



void CDsSejc::ComPareSe(TString strWebValue, TString strValue)
{
	double lfMoney = atof(boost::erase_all_copy(strWebValue, ",").c_str());

	double dMoney;

	if (strValue != "")  //入参中有XML税额
	{
		dMoney = atof(boost::erase_all_copy(strValue, ",").c_str());
	}
	else     //入参中无XML税额，则去XML文件中获取
	{
		if (!m_spCurrentTaskGroup->GetDeclareMoney(dMoney))
		{
			LOG_FUNC_INFO("已申报，xml里面没有税额属性，无法进行申报税额校验");
			NotifyDone(ExcuteFailed, "已申报，xml里面没有税额属性，无法进行申报税额校验");
			return;
		}
	}

	LOG_FUNC_INFO("报文中获取税款总额：【%.2lf】,网站获取税款总额：【%.2lf】", dMoney, lfMoney);
    double err =  1e-6;
    if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_SBDWJFR_SBB"))
    {
        err =  0.01 + 1e-6;
    }
	if (std::abs(dMoney - lfMoney) > err)
	{
		// 税额校验不一致.
		TString strMsg;
		CStringUtils::Format(strMsg, "该税种已申报，但税局税额与云帐房不一致，云帐房值：【%.2lf】，税局值：【%.2lf】,请核实！", dMoney, lfMoney);
		LOG_FUNC_INFO("%s", strMsg.c_str());
		NotifyDone(ExcuteFailedUser, strMsg);
	}
	else
	{
        if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_SBDWJFR_SBB"))
        {
            LOG_FUNC_INFO("回写社保【本期应缴费额】【%s】", strWebValue.c_str()); 
            BOOST_FOREACH(auto& Cell, m_pTaskCtrl->GetCurrentTaskItem()->m_vecWriteCells)
            {
                Cell.m_strValue = strWebValue; 
                if(Cell.m_mapAttrs.find("change")!= Cell.m_mapAttrs.end())
                {
                    LOG_FUNC_INFO("change 由0改为1");
                    Cell.m_mapAttrs["change"] = "1";
                }
            }
        }
#ifdef  _DEBUG

                   DeleteAllImage();
#endif

        NotifyDone(m_iCode, m_SnapshotData, true);
    }

}



void CDsSejc::RetryOpertaion(TString strMsg)
{
	//重新执行任务则需要将参数恢复到初始状态
	LOG_FUNC_INFO("%s", strMsg);
	if (m_iRetryOperation++ < MAX_RETRY)
	{
		m_bflag = false;
		LOG_FUNC_INFO("第%d次回到主页面重新打开税表进行税额校验", m_iRetryOperation);
		__super::TaskBegin();
	}
	else
	{
		NotifyDone(ExcuteFailed, "请重新下发任务!" + strMsg, false, true);
	}
}