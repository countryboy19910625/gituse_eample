#include "StdAfx.h"
#include <boost/foreach.hpp>
#include "DsSejc.h"
#include "common\StringUtils.h"
#include "libjson\Source\NumberToString.h"
#include "JsonTools.h"
#include <boost/foreach.hpp>

const int MAX_RETRY = 4;  //�����쳣��������·�4��
//我欲乘风归去，又恐琼楼玉宇，起舞弄清影，何似在人间
CDsSejc::~CDsSejc()
{
}

void CDsSejc::DoTask()
{
	//�Ƚ�ͼ����У��˰��
	LOG_FUNC_INFO("�걨״̬Ϊ���걨��ֱ�ӽ���˰��У��");

	TString strTaxName = IdToTaxName(m_spCurrentTaskGroup->m_strGroupId);   //����򿪵����걨ҳ��ı�������
	if (strTaxName.empty())
	{
		NotifyDone(ExcuteFailed, TEXT("��֧�ֵ�����ID����") + m_spCurrentTaskGroup->m_strGroupId + TEXT("��"));
		return;
	}

	//1����ͼ
	if (!GetHZPicBase64(NULL, m_SnapshotData))
	{
		NotifyDone(ExcuteTaxPro, "��ͼʧ�ܣ�");
		return;
	}
	LOG_FUNC_INFO("�걨��˰ҳ���걨��ɽ�ͼ�ɹ���");

	//2���ж��Ƿ���Ҫ��ִ
	TString rtnJson;
	IS_N_RET(LoadJsFile(EST_STATE_SBNS, IDR_JS_DS_CHECK_DECL), sLoadJsErrMsg);
	if (!InvokeScriptString(EST_STATE_SBNS, rtnJson, TEXT("SBNS_check_tax_decl"), strTaxName))
	{
		m_OpenWebStateID = -1;
		AddImage("δ�ҵ�", GetWebState(EST_STATE_SBNS)->GetHwnd());
		NotifyDone(ExcuteFailedTaxation, rtnJson);
		return;
	}
	TString strStatus = CJsonTools::GetJsonVal(rtnJson, TEXT("status"));
//	m_iCode = boost::contains(strStatus, "���걨(�ѵ���)") ? DeclaredDone : NeedRecvReceipt;
	m_iCode = DeclaredDone;

	//3����ȡ˰����Ϣ
	LOG_FUNC_INFO("��˰����%s����ȡ˰����Ϣ", strTaxName.c_str());
	if (!InvokeScriptString(EST_STATE_SBNS, rtnJson, TEXT("yzf_click_link_text"), TEXT("a"), strTaxName))
	{
		RetryOpertaion("�걨��˰ҳ�����걨��ʧ��");
		return;
	}
}


void CDsSejc::TimerHandle(UINT_PTR nIDEvent)
{
	TString strTaxName = IdToTaxName(m_spCurrentTaskGroup->m_strGroupId);   //����򿪵����걨ҳ��ı�������
	m_pTaskCtrl->KillTimer(nIDEvent);
	switch (nIDEvent)
	{
	case ID_TIMER_TAX_DECL_LIST_COMPLETE:
	{
		if (!m_bflag)
		{
			//��ʵ��ֻ��ӡ��˰�����м�ҳ��
			LOG_FUNC_INFO("��ǰʵ�ʴ��ڡ�%s���м�ҳ��", strTaxName.c_str());
			TString strItem = m_spCurrentTaskGroup->m_strSsqs + "~" + m_spCurrentTaskGroup->m_strSsqz;
			TString js = "$('a:contains(" + strItem + ")')[0].click()";
			if (!m_spCurrentState->ExcuteScript(js))
			{
				if (!m_spCurrentState->ExcuteScript(js))
				{
					NotifyDone(ExcuteFailed, "�������Ե������ޣ�����м�ҳ���б�ʧ��,�����Ƿ������ڲ��ԣ�");
					return;
				}
			}
		}
	}
	break;

	case ID_TIMER_TAX_DECL_TABLE_OPEN:
	{
		LOG_FUNC_INFO("��ǰʵ�ʴ��ڡ�%s������ҳ��", strTaxName.c_str());
		TString strMsg;
		//����򿪵�Ӧ�õ���ҳ���ϵ��걨��
		int iCount = 0;
		while (iCount++ < 3)
		{
			if (IsExistKeyWord(_T("�걨"), _T("a")))
			{
				break;
			}
			MsgSleep(3000);
		}
		if (iCount >= 3)
		{
			LOG_FUNC_INFO("��%s������ҳ�����ʧ�ܣ�", strTaxName.c_str());
			RetryOpertaion("����ҳ�����ʧ��");
			return;
		}

		if (!m_spCurrentState->ExcuteScript("$(\"a:contains(�걨)\")[0].click()"))
		{
			if (!m_spCurrentState->ExcuteScript("$(\"a:contains(�걨)\")[0].click()"))
			{
				RetryOpertaion("����ҳ�������걨��ʧ��");
				return;
			}
		}
	}
	break;

	case ID_TIMER_TAX_DECL_TABLE_COMPLETE:
	{
		LOG_FUNC_INFO("��ǰʵ�ʴ��ڡ�%s����ҳ��", strTaxName.c_str());

		//����һ��˰��������ȫ������ 
		MsgSleep(3000);
		int iCount = 0;
		while (iCount++ < 3)
		{
			if (IsExistKeyWord(_T("ȫ�걨"), _T("a")))
			{
				break;
			}
			MsgSleep(3000);
		}
		if (iCount >= 3)
		{
			LOG_FUNC_INFO("��%s����дҳ�����ʧ�ܣ�", strTaxName.c_str());
			RetryOpertaion("��дҳ�����ʧ��");
			return;
		}
		excute.liumengliuyongliulingliusiliushuynliushushengchenmeixiuzhangcuilan
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
		//DoNothing but cannot  be lacked//�걨��˰ҳ��,��ַ�ʹ�ҳ����ַ�в����غ�
	}
	else if (boost::contains(strUrl, TEXT("pages/sb/nssb/sb_dcsb.html")))  //�������м�ҳ��
	{
		m_pTaskCtrl->SetTimer(ID_TIMER_TAX_DECL_LIST_COMPLETE, SLEEP_AJAX_COMPLETE_TIME);
	}
	else if (boost::contains(strUrl, TEXT("sb/nssb/bdlb.html")))  //�����˵���˵��ҳ��
	{
		m_bflag = true;
		m_pTaskCtrl->SetTimer(ID_TIMER_TAX_DECL_TABLE_OPEN, SLEEP_AJAX_COMPLETE_TIME);
	}
	else if (boost::contains(strUrl, TEXT("/zjgfcsszjdzswjsbweb/pages/sb"))
		|| boost::contains(strUrl, TEXT("zjgfzjdzswjsbweb/pages/sb"))
		|| boost::contains(strUrl, TEXT("dzswjsbwebxyw/pages/sb")))  //�����ҳ��
	{
		m_pTaskCtrl->SetTimer(ID_TIMER_TAX_DECL_TABLE_COMPLETE, SLEEP_AJAX_COMPLETE_TIME);
	}

	return S_OK;
}


void CDsSejc::SeJc()
{
	LOG_FUNC_INFO("����ȡ���ұȶ�˰�");
	TString strMsg;
	TString strSz = TEXT("");
	TString strWebValue = "";

	if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_CJRJYBZJ_SBB")) { strSz = TEXT("�б���"); }
	if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_TY_SBB")) { strSz = TEXT("ͨ���걨"); }
	if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_SBDWJFR_SBB")) { strSz = TEXT("�籣"); }

	if ( m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_CJRJYBZJ_SBB")
		|| m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_TY_SBB"))
	{
		const TString strTaskId = boost::algorithm::erase_all_copy(m_taskId, ID_SEJC);
		const auto& taskItem = m_pTaskCtrl->GetTaskItemById(strTaskId);
		BOOST_FOREACH(auto& cell, taskItem->m_vecVerifyCells)
		{
			TString yspzmc = cell.m_strAttrId;     //Ӧ˰ƾ֤����
			TString column = cell.m_mapAttrs["col"];
			TString TaxName = cell.m_strTaxName;

			//������Ϣ�ҵ���Ӧ��Ԫ��
			if (cell.m_mapAttrs.find("se") != cell.m_mapAttrs.end() && "true" == cell.m_mapAttrs["se"])
			{
				std::vector<TString> vecParam;
				vecParam.push_back(yspzmc);

				//ӡ��˰���е�����걨���ɸ����б�ɹ̶���
				if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_YHS_SBB"))
				{
					int col = atoi(column.c_str()) - 1;
					CStringUtils::Format(column, "%d", col);
				}
				vecParam.push_back(column);
				vecParam.push_back(strSz);

				if (!ExcuteJsFunc(EID_STATE_TAX_OPENED_TABLE, TEXT("YzfGetCellValue"), vecParam, strWebValue))
				{
					CStringUtils::Format(strMsg, "��Ԫ��%s��ȡֵ����:��%s��", TaxName.c_str(), strWebValue.c_str());
					LOG_FUNC_ERR("%s", strMsg.c_str());
					NotifyDone(ExcuteFailed, strMsg);
					return;
				}
				break;
			}
		}
		if (strWebValue == "")
		{
			LOG_FUNC_INFO("��ȡ˰��˰��ֵΪ�գ����飡");
			NotifyDone(ExcuteFailedUser, "��ȡ˰��˰��ֵΪ�գ����飡", false, true);
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
				LOG_FUNC_INFO("û�м�⵽�ؼ�Ԫ�أ��޷���ȡ˰��˰��");
				NotifyDone(ExcuteFailed, _T("û�м�⵽�ؼ�Ԫ�أ��޷���ȡ˰��˰��"));
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
				fz = "�ϼ�";
				strCol = strId;
				TString rtnJson;
				if (!ExecScriptString(EID_STATE_TAX_OPENED_TABLE, rtnJson, TEXT("BlankCell_Value_Get_Shbx"), fz, zspm, zszm, strCol))
				{
					return;
				}
				strWebValue = CJsonTools::GetJsonVal(rtnJson, "value");
				if (strWebValue == "")
				{
					LOG_FUNC_INFO("��ȡ˰��˰��ֵΪ�գ����飡");
					NotifyDone(ExcuteFailedUser, "��ȡ˰��˰��ֵΪ�գ����飡", false, true);
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
				LOG_FUNC_INFO("û�м�⵽�ؼ�Ԫ�أ��޷���ȡ˰��˰��");
				NotifyDone(ExcuteFailed, _T("û�м�⵽�ؼ�Ԫ�أ��޷���ȡ˰��˰��"));
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
			LOG_FUNC_INFO("���걨����δ��鵽�������е�˰�����ԣ��޷������걨˰��У�飬���鱨����");
			NotifyDone(ExcuteFailedUser, "���걨����δ��鵽�������е�˰�����ԣ��޷������걨˰��У�飬���鱨����");
			return;
		}

		LOG_FUNC_INFO("���걨У��:��ȡ˰��Ӧ����˰��ֵ");
		GetInnerText(EID_STATE_TAX_OPENED_TABLE, "#bqybtse_hj", strWebValue);
		if (strWebValue == "")
		{
			LOG_FUNC_INFO("��ȡ˰��˰��ֵΪ�գ����飡");
			NotifyDone(ExcuteFailedUser, "��ȡ˰��˰��ֵΪ�գ����飡", false, true);
			return;
		}
		ComPareSe(strWebValue, strSe);
	}
}



void CDsSejc::ComPareSe(TString strWebValue, TString strValue)
{
	double lfMoney = atof(boost::erase_all_copy(strWebValue, ",").c_str());

	double dMoney;

	if (strValue != "")  //�������XML˰��
	{
		dMoney = atof(boost::erase_all_copy(strValue, ",").c_str());
	}
	else     //�������XML˰���ȥXML�ļ��л�ȡ
	{
		if (!m_spCurrentTaskGroup->GetDeclareMoney(dMoney))
		{
			LOG_FUNC_INFO("���걨��xml����û��˰�����ԣ��޷������걨˰��У��");
			NotifyDone(ExcuteFailed, "���걨��xml����û��˰�����ԣ��޷������걨˰��У��");
			return;
		}
	}

	LOG_FUNC_INFO("�����л�ȡ˰���ܶ��%.2lf��,��վ��ȡ˰���ܶ��%.2lf��", dMoney, lfMoney);
    double err =  1e-6;
    if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_SBDWJFR_SBB"))
    {
        err =  0.01 + 1e-6;
    }
	if (std::abs(dMoney - lfMoney) > err)
	{
		// ˰��У�鲻һ��.
		TString strMsg;
		CStringUtils::Format(strMsg, "��˰�����걨����˰��˰�������ʷ���һ�£����ʷ�ֵ����%.2lf����˰��ֵ����%.2lf��,���ʵ��", dMoney, lfMoney);
		LOG_FUNC_INFO("%s", strMsg.c_str());
		NotifyDone(ExcuteFailedUser, strMsg);
	}
	else
	{
        if (m_spCurrentTaskGroup->m_strGroupId == _T("ZHEJIANGDS_SBDWJFR_SBB"))
        {
            LOG_FUNC_INFO("��д�籣������Ӧ�ɷѶ��%s��", strWebValue.c_str()); 
            BOOST_FOREACH(auto& Cell, m_pTaskCtrl->GetCurrentTaskItem()->m_vecWriteCells)
            {
                Cell.m_strValue = strWebValue; 
                if(Cell.m_mapAttrs.find("change")!= Cell.m_mapAttrs.end())
                {
                    LOG_FUNC_INFO("change ��0��Ϊ1");
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
	//����ִ����������Ҫ�������ָ�����ʼ״̬
	LOG_FUNC_INFO("%s", strMsg);
	if (m_iRetryOperation++ < MAX_RETRY)
	{
		m_bflag = false;
		LOG_FUNC_INFO("��%d�λص���ҳ�����´�˰������˰��У��", m_iRetryOperation);
		__super::TaskBegin();
	}
	else
	{
		NotifyDone(ExcuteFailed, "�������·�����!" + strMsg, false, true);
	}
}
