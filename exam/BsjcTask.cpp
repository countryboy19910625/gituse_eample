#include "StdAfx.h"
#include "BsjcTask.h"

#include <boost/property_tree/json_parser.hpp>
#include "JsonTools.h"

#include "GetSzTask.h"
#include "DbjcTask.h"


using namespace boost::property_tree;
using namespace std;

enum ID_STATIC_TASK_TIMER
{
	ID_SBNS_TIMER_Receipt = OperationTimer + 20000,

};

CBsjcTask::~CBsjcTask()
{
}


void CBsjcTask::TimerHandle(UINT_PTR nIDEvent)
{
	switch (nIDEvent)
	{

	case ID_SBNS_TIMER_Receipt:
		m_pTaskCtrl->KillTimer(nIDEvent);
		{
			TString strHZPicBase64;
			if (GetHZPicBase64(NULL, strHZPicBase64, m_strLbxx))
			{
				NotifyDone(CheckTaxMissing, strHZPicBase64, true);
			}
			else
			{
				NotifyDone(ExcuteFailed, "截图失败",false,true);
			}
		}
		break;

	default:
		__super::TimerHandle(nIDEvent);
		break;
	}
}

void CBsjcTask::DoTask()
{
	m_OpenWebStateID = EST_STATE_SBNS;

	TString strDescript; //报税遗漏描述

	//获取当前页面已申报信息
	TString strMsg;
	TString rtnJson;
	IS_N_RET(LoadJsFile(EST_STATE_SBNS, IDR_JS_DS_CHECK_DECL), sLoadJsErrMsg);

	if (!ExecScriptString(EST_STATE_SBNS, rtnJson, TEXT("Get_tax_decl_Inof")))
	{
		NotifyDone(ExcuteFailed, rtnJson);
		return;
	}

	std::list<std::map<TString, TString>> CollectedInfosList;
	if (!CJsonTools::JsonToTaskParamItem(rtnJson, CollectedInfosList) || CollectedInfosList.empty())
	{
		strMsg = TEXT("申报信息结果解析失败");
		LOG_FUNC_ERR("%s", strMsg.c_str());
		NotifyDone(ExcuteFailed, strMsg);
		return;
	}


	//对比需要申报税种在纳税申报信息
	std::list<std::map<TString, TString>>  ToCheck = CGetSzTask::vecToCheck;
	for (auto it = ToCheck.begin(); it != ToCheck.end(); it++)
	{
		//根据征收项目名称费空，然后根据纳税期限名称
		std::map<TString, TString> cell = (*it);
		TString strZsxmmcName = cell["zsxmmc"];
		TString strZspmmcName = cell["zspmmc"];
		TString strDestNsqx = cell["nsqx"];
		LOG_FUNC_INFO("检查：项目名称【%s】,品目名称【%s】,纳税期限【%s】的税目申报状态", strZsxmmcName.c_str(), strZspmmcName.c_str(), strDestNsqx.c_str());


		//映射获取表名 TODO
		bool bReportNameMtach = false;
		for (auto itt = CollectedInfosList.begin(); itt != CollectedInfosList.end(); itt++)
		{
			TString strSrcName = (*itt)["name"];
			TString strSrcSsqq = (*itt)["ssqq"];
			TString strSrcSsqz = (*itt)["ssqz"];
			TString strStatus = (*itt)["status"];

			if (strZsxmmcName == "增值税" && 
				(strSrcName == "增值税一般纳税人申报" || strSrcName == "增值税小规模纳税人（非定期定额户）申报"))    //TODO
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "企业所得税" &&
				(strSrcName == "居民企业所得税月（季）度预缴纳税申报（A类，2018版）"
					|| strSrcName == "居民企业所得税月（季）度预缴纳税申报（B类，2018版）"))  //TODO
			{
				bReportNameMtach = true;
			}
			else if ((strZsxmmcName == "城市维护建设税" 
				|| strZsxmmcName == "教育费附加"  
				|| strZsxmmcName == "地方教育附加")
				&& strSrcName == "城建税、教育费附加、地方教育附加税（费）申报表" )
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "文化事业建设费"
				&&	strSrcName == "文化事业建设费申报")
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "消费税"
				&&	strSrcName == "应税消费品消费税申报")
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "印花税" 
				&&	strSrcName == "印花税申报" )
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "残疾人就业保障金"
				&&	strSrcName == "残疾人就业保障金缴费申报")
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "其他收入" && (strZspmmcName == "工会经费" || strZspmmcName == "工会筹备金")
				&&	strSrcName.find("通用申报表")!=-1)
			{
				bReportNameMtach = true;
			}

		    else if (strZsxmmcName == "个人所得税" && strZspmmcName == "个体户生产经营所得"  
				&&	(strSrcName == "个人所得税生产经营所得纳税申报表（A表）"|| strSrcName == "定期定额个体工商户纳税分月（季）汇总申报"))
			{
				bReportNameMtach = true;
			}

			if (bReportNameMtach)
			{
				if (strSrcSsqq == "")  //所属期为空，直接检查状态
				{
					LOG_FUNC_INFO("【%s】的申报状态为【%s】", strSrcName.c_str(), strStatus.c_str());
					if (strStatus.find("已申报(已导入)") == -1 && strStatus != ""&& strStatus.find("其他途径已申报") == -1)
					{
						CStringUtils::Format(strMsg, "【%s】的申报状态异常:【%s】\n", strSrcName.c_str(), strStatus.c_str());
						strDescript += strMsg;
					}
				}
				else
				{
			        //所属期不为空需要比对纳税期限

					int monthBegin = atoi(strSrcSsqq.substr(5, 2).c_str());
					int monthEnd = atoi(strSrcSsqz.substr(5, 2).c_str());
					TString strSrcNsqx;

					if ( strSrcSsqq == strSrcSsqz)
					{
						strSrcNsqx = "次";
					}
					else if (monthBegin == monthEnd && strSrcSsqq != strSrcSsqz)
					{
						strSrcNsqx = "月";
					}
					else if (monthEnd - monthBegin == 2 && strSrcSsqq != strSrcSsqz)
					{
						strSrcNsqx = "季";
					}
					else if (monthEnd - monthBegin == 11 && strSrcSsqq != strSrcSsqz)
					{
						strSrcNsqx = "年";
					}


					if (strDestNsqx != strSrcNsqx)
					{
						CStringUtils::Format(strMsg, "【%s】唯一匹配，但申报日期【%s-%s】与鉴定信息纳税期限【%s】不一致\n",
							strSrcName.c_str(), strSrcSsqq.c_str(), strSrcSsqz.c_str(), strDestNsqx.c_str());
						LOG_FUNC_INFO("%s", strMsg.c_str());
						strDescript += strMsg;
					}
					else
					{
						if (strStatus.find("已申报(已导入)") != -1 || strStatus.find("其他途径已申报") != -1)
						{
							LOG_FUNC_INFO("【%s】唯一匹配，申报状态正常【已申报(已导入)】", strSrcName.c_str());
						}
						else
						{
							CStringUtils::Format(strMsg, "【%s】的申报状态异常为:【%s】\n", strSrcName.c_str(), strStatus.c_str());
							LOG_FUNC_INFO("%s", strMsg.c_str());
							strDescript += strMsg;
						}
					}					
				}	
				break;
			}

		}
		if (!bReportNameMtach)
		{
			CStringUtils::Format(strMsg, "鉴定信息中【%s-%s-%s】在纳税申报页面查找不到对应报表\n", strZsxmmcName.c_str(), strZspmmcName.c_str(), strDestNsqx.c_str());
			LOG_FUNC_INFO("%s", strMsg.c_str());
			strDescript += strMsg;
		}
	}


	//进一步检查社保和财务报表
	for (auto itt = CollectedInfosList.begin(); itt != CollectedInfosList.end(); itt++)
	{
		TString strSrcName = (*itt)["name"];
		TString strSrcSsqq = (*itt)["ssqq"];
		TString strSrcSsqz = (*itt)["ssqz"];
		TString strStatus = (*itt)["status"];


		//TO DO保险起见，防止在常规申报页面有社保补缴或居民企业参股但是我的待办里没有

		if (strSrcName.find("社会保险费补缴申报表") != -1)
		{
			if (CDbjcTask::bSbbj)
			{
				continue;   //检查过了就跳过
			}
			else
			{
				CStringUtils::Format(strMsg, "【%s】的申报状态异常为:【%s】\n", strSrcName.c_str(), strStatus.c_str());
				LOG_FUNC_INFO("%s", strMsg.c_str());
				strDescript += strMsg;
				continue;
			}
		}


		if (strSrcName.find("财务报表") != -1)
		{
			if (strSrcSsqq.find("01-01") != -1 && strSrcSsqz.find("12-31") != -1 && m_bCheckCBYear == "false")
			{
				continue;
			}
		}
		else if (strSrcName.find("社会保险费缴费申报表（适用单位缴费人）") == -1 )
		{
			continue;
		}

		//剩下的为非年报财报和社保
		if (strStatus.find("已申报(已导入)") == -1 && strStatus.find("其他途径已申报") == -1)
		{
			CStringUtils::Format(strMsg, "【%s】的申报状态异常为:【%s】\n", strSrcName.c_str(), strStatus.c_str());
			LOG_FUNC_INFO("%s", strMsg.c_str());
			strDescript += strMsg;
		}
		LOG_FUNC_INFO("发现常规申报页面的【%s】申报状态为：【%s】", strSrcName.c_str() , strStatus.c_str());
	}



	LOG_FUNC_INFO("再次整体检查");
	std::vector<TString> vecParam;
	vecParam.push_back(m_bCheckCBYear);
	if (!ExcuteJsFunc(EST_STATE_SBNS, TEXT("SBNS_check_tax_decl_ALL_1"), vecParam, strMsg))
	{
		LOG_FUNC_INFO("%s", strMsg.c_str());
		NotifyDone(ExcuteFailed, strMsg);
		return;
	}

	if (!boost::contains(strMsg, "不存在漏报"))
	{
		LOG_FUNC_INFO("%s", strMsg.c_str());
		strDescript += strMsg;   //存在漏报的描述写入图片
	}

	LOG_FUNC_INFO("常规页面检查结果：%s", strDescript.c_str());
	m_strLbxx += strDescript;
	LOG_FUNC_INFO("所有检查结果汇总%s", m_strLbxx.c_str());

	if (m_strLbxx != "")
	{	
		AddTaxMissImage("所有申报检查结果汇总", NULL, m_strLbxx);
	}
	else
	{
		AddTotalImage("所有申报检查结果汇总", NULL, m_strLbxx);
	}

	//到这里再检查一下税款缴纳情况
	LOG_FUNC_INFO("跳转【扣款结果查询】页面进行检查");
	if (!GetWebState(EST_STATE_SBNS)->ExcuteScript("$(\"a:contains(扣款结果查询)\")[0].click();"))
	{
		NotifyDone(ExcuteFailed, "点击【扣款结果查询】失败");
		return;
	}
	MsgSleep(1000);

	
	IS_N_RET(LoadJsFile(EST_STATE_SBNS, IDR_JS_DS_CHECK_DECL), sLoadJsErrMsg);
	if (!ExecScriptString(EST_STATE_SBNS, rtnJson, TEXT("BJJC_check_tax_KK")))
	{
		return;
	}
	std::list<std::map<TString, TString>> mapCollectedInfosList;
	if (!CJsonTools::JsonToTaskParamItem(rtnJson, mapCollectedInfosList))
	{
		LOG_FUNC_ERR("Ajax请求结果解析失败");
		NotifyDone(ExcuteFailed, "Ajax请求结果解析失败");
		return ;
	}

	TString strKkxx;
	for (auto it = mapCollectedInfosList.begin(); it != mapCollectedInfosList.end(); it++)
	{
		bool bZdflag = false;
		bool bSdflag = false;
		TString strName = (*it)["name"];
		if (strName.find("中华人民共和国企业所得税年度纳税申报表")!=-1)
		{
			LOG_FUNC_INFO("跳过《中华人民共和国企业所得税年度纳税申报表》扣款状态检查");
			continue;
		}

		//看是否提示的扣款信息是否包含成功
		TString strWebKkxx = (*it)["ZD_zfxx"];
		if (((boost::contains(strWebKkxx, "000") && boost::contains(strWebKkxx, "成功")) || boost::contains(strWebKkxx, "扣款成功") || boost::contains(strWebKkxx, "交易成功") || boost::contains(strWebKkxx, "SUCCESS"))
			&& !boost::contains(strWebKkxx, "否") && !boost::contains(strWebKkxx, "没")
			&& !boost::contains(strWebKkxx, "不")
			&& !boost::contains(strWebKkxx, "未")
			&& !boost::contains(strWebKkxx, "失败")
			&& !boost::contains(strWebKkxx, "等待")
			&& !boost::contains(strWebKkxx, "刷新")
			&& !boost::contains(strWebKkxx, "封锁")
			&& !boost::contains(strWebKkxx, "状态"))
		{
			bZdflag = true;
		}

		TString strWebKkxx1 =  (*it)["SD_zfxx"];
		if (((boost::contains(strWebKkxx1, "000") && boost::contains(strWebKkxx1, "成功")) || boost::contains(strWebKkxx1, "扣款成功") || boost::contains(strWebKkxx1, "交易成功") || boost::contains(strWebKkxx1, "SUCCESS"))
			&& !boost::contains(strWebKkxx1, "否") && !boost::contains(strWebKkxx1, "没")
			&& !boost::contains(strWebKkxx1, "不")
			&& !boost::contains(strWebKkxx1, "未")
			&& !boost::contains(strWebKkxx1, "失败")
			&& !boost::contains(strWebKkxx1, "等待")
			&& !boost::contains(strWebKkxx1, "刷新")
			&& !boost::contains(strWebKkxx1, "封锁")
			&& !boost::contains(strWebKkxx1, "状态"))
		{
			bSdflag = true;
		}
		if (!bZdflag && !bSdflag)
		{
			strKkxx = strKkxx + "税目【" + strName + "】的扣款状态不正确\n";
		}
	}

	m_strLbxx += strKkxx;
	if (strKkxx != "")
	{
		LOG_FUNC_INFO("%s", strKkxx.c_str());
		AddTaxMissImage("已扣款页面漏报", NULL, strKkxx);
	}
	else
	{
		LOG_FUNC_INFO("已扣款界面扣款状态均正常");
		AddImage("已扣款界面结果", NULL);
	}
	
	//再去检查一下未扣款信息页面
	LOG_FUNC_INFO("跳转【税款缴纳】页面进行检查");
	if (!GetWebState(EST_STATE_SBNS)->ExcuteScript("$(\"a:contains(税款缴纳)\")[0].click();"))
	{
		NotifyDone(ExcuteFailed, "点击【税款缴纳】失败");
		return ;
	}
	MsgSleep(3000);

	TString strWkkxx;
	if (!IsExistKeyWord("暂无欠税信息","span"))
	{
		strWkkxx = "税款缴纳页面存在尚未缴款项目，请检查！！";
		LOG_FUNC_INFO("%s", strWkkxx.c_str());
		AddTaxMissImage("未扣款页面漏报", NULL, strWkkxx);
	}
	else
	{
		LOG_FUNC_INFO("未扣款界面结果均正常");
		AddImage("未扣款界面结果", NULL);
	}
	m_strLbxx += strWkkxx;

	if (m_strLbxx != "")
	{
        if(m_strLbxx.length() > 90)
        {
            m_strLbxx = m_strLbxx.substr(0,90);
        }
		NotifyDone(CheckTaxMissing, m_strLbxx);
	}
	else
	{
		NotifyDone(ExcuteSuccess, "国地税检查完成，未发现漏报");
	}
	
}