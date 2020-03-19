#pragma once
#include "DsOpenTaxListTask.h"

class CBsjcTask : public CDsOpenTaxListTask
{
public:
	explicit CBsjcTask(const TString& id)
		: CDsOpenTaxListTask(id){}

	~CBsjcTask();

	void DoTask() override;
 	                 
	void TimerHandle(UINT_PTR nIDEvent) override;
	
	
	#this is what I have fixed here 

	
};

