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
				NotifyDone(ExcuteFailed, "��ͼʧ��",false,true);
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

	TString strDescript; //��˰��©����

	//��ȡ��ǰҳ�����걨��Ϣ
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
		strMsg = TEXT("�걨��Ϣ�������ʧ��");
		LOG_FUNC_ERR("%s", strMsg.c_str());
		NotifyDone(ExcuteFailed, strMsg);
		return;
	}


	//�Ա���Ҫ�걨˰������˰�걨��Ϣ
	std::list<std::map<TString, TString>>  ToCheck = CGetSzTask::vecToCheck;
	for (auto it = ToCheck.begin(); it != ToCheck.end(); it++)
	{
		//����������Ŀ���Ʒѿգ�Ȼ�������˰��������
		std::map<TString, TString> cell = (*it);
		TString strZsxmmcName = cell["zsxmmc"];
		TString strZspmmcName = cell["zspmmc"];
		TString strDestNsqx = cell["nsqx"];
		LOG_FUNC_INFO("��飺��Ŀ���ơ�%s��,ƷĿ���ơ�%s��,��˰���ޡ�%s����˰Ŀ�걨״̬", strZsxmmcName.c_str(), strZspmmcName.c_str(), strDestNsqx.c_str());


		//ӳ���ȡ���� TODO
		bool bReportNameMtach = false;
		for (auto itt = CollectedInfosList.begin(); itt != CollectedInfosList.end(); itt++)
		{
			TString strSrcName = (*itt)["name"];
			TString strSrcSsqq = (*itt)["ssqq"];
			TString strSrcSsqz = (*itt)["ssqz"];
			TString strStatus = (*itt)["status"];

			if (strZsxmmcName == "��ֵ˰" && 
				(strSrcName == "��ֵ˰һ����˰���걨" || strSrcName == "��ֵ˰С��ģ��˰�ˣ��Ƕ��ڶ�����걨"))    //TODO
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "��ҵ����˰" &&
				(strSrcName == "������ҵ����˰�£�������Ԥ����˰�걨��A�࣬2018�棩"
					|| strSrcName == "������ҵ����˰�£�������Ԥ����˰�걨��B�࣬2018�棩"))  //TODO
			{
				bReportNameMtach = true;
			}
			else if ((strZsxmmcName == "����ά������˰" 
				|| strZsxmmcName == "�����Ѹ���"  
				|| strZsxmmcName == "�ط���������")
				&& strSrcName == "�ǽ�˰�������Ѹ��ӡ��ط���������˰���ѣ��걨��" )
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "�Ļ���ҵ�����"
				&&	strSrcName == "�Ļ���ҵ������걨")
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "����˰"
				&&	strSrcName == "Ӧ˰����Ʒ����˰�걨")
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "ӡ��˰" 
				&&	strSrcName == "ӡ��˰�걨" )
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "�м��˾�ҵ���Ͻ�"
				&&	strSrcName == "�м��˾�ҵ���Ͻ�ɷ��걨")
			{
				bReportNameMtach = true;
			}

			else if (strZsxmmcName == "��������" && (strZspmmcName == "���ᾭ��" || strZspmmcName == "����ﱸ��")
				&&	strSrcName.find("ͨ���걨��")!=-1)
			{
				bReportNameMtach = true;
			}

		    else if (strZsxmmcName == "��������˰" && strZspmmcName == "���廧������Ӫ����"  
				&&	(strSrcName == "��������˰������Ӫ������˰�걨��A��"|| strSrcName == "���ڶ�����幤�̻���˰���£����������걨"))
			{
				bReportNameMtach = true;
			}

			if (bReportNameMtach)
			{
				if (strSrcSsqq == "")  //������Ϊ�գ�ֱ�Ӽ��״̬
				{
					LOG_FUNC_INFO("��%s�����걨״̬Ϊ��%s��", strSrcName.c_str(), strStatus.c_str());
					if (strStatus.find("���걨(�ѵ���)") == -1 && strStatus != ""&& strStatus.find("����;�����걨") == -1)
					{
						CStringUtils::Format(strMsg, "��%s�����걨״̬�쳣:��%s��\n", strSrcName.c_str(), strStatus.c_str());
						strDescript += strMsg;
					}
				}
				else
				{
			        //�����ڲ�Ϊ����Ҫ�ȶ���˰����

					int monthBegin = atoi(strSrcSsqq.substr(5, 2).c_str());
					int monthEnd = atoi(strSrcSsqz.substr(5, 2).c_str());
					TString strSrcNsqx;

					if ( strSrcSsqq == strSrcSsqz)
					{
						strSrcNsqx = "��";
					}
					else if (monthBegin == monthEnd && strSrcSsqq != strSrcSsqz)
					{
						strSrcNsqx = "��";
					}
					else if (monthEnd - monthBegin == 2 && strSrcSsqq != strSrcSsqz)
					{
						strSrcNsqx = "��";
					}
					else if (monthEnd - monthBegin == 11 && strSrcSsqq != strSrcSsqz)
					{
						strSrcNsqx = "��";
					}


					if (strDestNsqx != strSrcNsqx)
					{
						CStringUtils::Format(strMsg, "��%s��Ψһƥ�䣬���걨���ڡ�%s-%s���������Ϣ��˰���ޡ�%s����һ��\n",
							strSrcName.c_str(), strSrcSsqq.c_str(), strSrcSsqz.c_str(), strDestNsqx.c_str());
						LOG_FUNC_INFO("%s", strMsg.c_str());
						strDescript += strMsg;
					}
					else
					{
						if (strStatus.find("���걨(�ѵ���)") != -1 || strStatus.find("����;�����걨") != -1)
						{
							LOG_FUNC_INFO("��%s��Ψһƥ�䣬�걨״̬���������걨(�ѵ���)��", strSrcName.c_str());
						}
						else
						{
							CStringUtils::Format(strMsg, "��%s�����걨״̬�쳣Ϊ:��%s��\n", strSrcName.c_str(), strStatus.c_str());
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
			CStringUtils::Format(strMsg, "������Ϣ�С�%s-%s-%s������˰�걨ҳ����Ҳ�����Ӧ����\n", strZsxmmcName.c_str(), strZspmmcName.c_str(), strDestNsqx.c_str());
			LOG_FUNC_INFO("%s", strMsg.c_str());
			strDescript += strMsg;
		}
	}


	//��һ������籣�Ͳ��񱨱�
	for (auto itt = CollectedInfosList.begin(); itt != CollectedInfosList.end(); itt++)
	{
		TString strSrcName = (*itt)["name"];
		TString strSrcSsqq = (*itt)["ssqq"];
		TString strSrcSsqz = (*itt)["ssqz"];
		TString strStatus = (*itt)["status"];


		//TO DO�����������ֹ�ڳ����걨ҳ�����籣���ɻ������ҵ�ιɵ����ҵĴ�����û��

		if (strSrcName.find("��ᱣ�շѲ����걨��") != -1)
		{
			if (CDbjcTask::bSbbj)
			{
				continue;   //�����˾�����
			}
			else
			{
				CStringUtils::Format(strMsg, "��%s�����걨״̬�쳣Ϊ:��%s��\n", strSrcName.c_str(), strStatus.c_str());
				LOG_FUNC_INFO("%s", strMsg.c_str());
				strDescript += strMsg;
				continue;
			}
		}


		if (strSrcName.find("���񱨱�") != -1)
		{
			if (strSrcSsqq.find("01-01") != -1 && strSrcSsqz.find("12-31") != -1 && m_bCheckCBYear == "false")
			{
				continue;
			}
		}
		else if (strSrcName.find("��ᱣ�շѽɷ��걨�����õ�λ�ɷ��ˣ�") == -1 )
		{
			continue;
		}

		//ʣ�µ�Ϊ���걨�Ʊ����籣
		if (strStatus.find("���걨(�ѵ���)") == -1 && strStatus.find("����;�����걨") == -1)
		{
			CStringUtils::Format(strMsg, "��%s�����걨״̬�쳣Ϊ:��%s��\n", strSrcName.c_str(), strStatus.c_str());
			LOG_FUNC_INFO("%s", strMsg.c_str());
			strDescript += strMsg;
		}
		LOG_FUNC_INFO("���ֳ����걨ҳ��ġ�%s���걨״̬Ϊ����%s��", strSrcName.c_str() , strStatus.c_str());
	}



	LOG_FUNC_INFO("�ٴ�������");
	std::vector<TString> vecParam;
	vecParam.push_back(m_bCheckCBYear);
	if (!ExcuteJsFunc(EST_STATE_SBNS, TEXT("SBNS_check_tax_decl_ALL_1"), vecParam, strMsg))
	{
		LOG_FUNC_INFO("%s", strMsg.c_str());
		NotifyDone(ExcuteFailed, strMsg);
		return;
	}

	if (!boost::contains(strMsg, "������©��"))
	{
		LOG_FUNC_INFO("%s", strMsg.c_str());
		strDescript += strMsg;   //����©��������д��ͼƬ
	}

	LOG_FUNC_INFO("����ҳ��������%s", strDescript.c_str());
	m_strLbxx += strDescript;
	LOG_FUNC_INFO("���м��������%s", m_strLbxx.c_str());

	if (m_strLbxx != "")
	{	
		AddTaxMissImage("�����걨���������", NULL, m_strLbxx);
	}
	else
	{
		AddTotalImage("�����걨���������", NULL, m_strLbxx);
	}

	//�������ټ��һ��˰��������
	LOG_FUNC_INFO("��ת���ۿ�����ѯ��ҳ����м��");
	if (!GetWebState(EST_STATE_SBNS)->ExcuteScript("$(\"a:contains(�ۿ�����ѯ)\")[0].click();"))
	{
		NotifyDone(ExcuteFailed, "������ۿ�����ѯ��ʧ��");
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
		LOG_FUNC_ERR("Ajax����������ʧ��");
		NotifyDone(ExcuteFailed, "Ajax����������ʧ��");
		return ;
	}

	TString strKkxx;
	for (auto it = mapCollectedInfosList.begin(); it != mapCollectedInfosList.end(); it++)
	{
		bool bZdflag = false;
		bool bSdflag = false;
		TString strName = (*it)["name"];
		if (strName.find("�л����񹲺͹���ҵ����˰�����˰�걨��")!=-1)
		{
			LOG_FUNC_INFO("�������л����񹲺͹���ҵ����˰�����˰�걨���ۿ�״̬���");
			continue;
		}

		//���Ƿ���ʾ�Ŀۿ���Ϣ�Ƿ�����ɹ�
		TString strWebKkxx = (*it)["ZD_zfxx"];
		if (((boost::contains(strWebKkxx, "000") && boost::contains(strWebKkxx, "�ɹ�")) || boost::contains(strWebKkxx, "�ۿ�ɹ�") || boost::contains(strWebKkxx, "���׳ɹ�") || boost::contains(strWebKkxx, "SUCCESS"))
			&& !boost::contains(strWebKkxx, "��") && !boost::contains(strWebKkxx, "û")
			&& !boost::contains(strWebKkxx, "��")
			&& !boost::contains(strWebKkxx, "δ")
			&& !boost::contains(strWebKkxx, "ʧ��")
			&& !boost::contains(strWebKkxx, "�ȴ�")
			&& !boost::contains(strWebKkxx, "ˢ��")
			&& !boost::contains(strWebKkxx, "����")
			&& !boost::contains(strWebKkxx, "״̬"))
		{
			bZdflag = true;
		}

		TString strWebKkxx1 =  (*it)["SD_zfxx"];
		if (((boost::contains(strWebKkxx1, "000") && boost::contains(strWebKkxx1, "�ɹ�")) || boost::contains(strWebKkxx1, "�ۿ�ɹ�") || boost::contains(strWebKkxx1, "���׳ɹ�") || boost::contains(strWebKkxx1, "SUCCESS"))
			&& !boost::contains(strWebKkxx1, "��") && !boost::contains(strWebKkxx1, "û")
			&& !boost::contains(strWebKkxx1, "��")
			&& !boost::contains(strWebKkxx1, "δ")
			&& !boost::contains(strWebKkxx1, "ʧ��")
			&& !boost::contains(strWebKkxx1, "�ȴ�")
			&& !boost::contains(strWebKkxx1, "ˢ��")
			&& !boost::contains(strWebKkxx1, "����")
			&& !boost::contains(strWebKkxx1, "״̬"))
		{
			bSdflag = true;
		}
		if (!bZdflag && !bSdflag)
		{
			strKkxx = strKkxx + "˰Ŀ��" + strName + "���Ŀۿ�״̬����ȷ\n";
		}
	}

	m_strLbxx += strKkxx;
	if (strKkxx != "")
	{
		LOG_FUNC_INFO("%s", strKkxx.c_str());
		AddTaxMissImage("�ѿۿ�ҳ��©��", NULL, strKkxx);
	}
	else
	{
		LOG_FUNC_INFO("�ѿۿ����ۿ�״̬������");
		AddImage("�ѿۿ������", NULL);
	}
	
	//��ȥ���һ��δ�ۿ���Ϣҳ��
	LOG_FUNC_INFO("��ת��˰����ɡ�ҳ����м��");
	if (!GetWebState(EST_STATE_SBNS)->ExcuteScript("$(\"a:contains(˰�����)\")[0].click();"))
	{
		NotifyDone(ExcuteFailed, "�����˰����ɡ�ʧ��");
		return ;
	}
	MsgSleep(3000);

	TString strWkkxx;
	if (!IsExistKeyWord("����Ƿ˰��Ϣ","span"))
	{
		strWkkxx = "˰�����ҳ�������δ�ɿ���Ŀ�����飡��";
		LOG_FUNC_INFO("%s", strWkkxx.c_str());
		AddTaxMissImage("δ�ۿ�ҳ��©��", NULL, strWkkxx);
	}
	else
	{
		LOG_FUNC_INFO("δ�ۿ������������");
		AddImage("δ�ۿ������", NULL);
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
		NotifyDone(ExcuteSuccess, "����˰�����ɣ�δ����©��");
	}
	
}