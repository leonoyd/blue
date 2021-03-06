/** ***************************************************************************
* @file
* @author Mike Sharkey <mike@pikeaero.com>.
* @copyright © 2005-2013 by Pike Aerospace Research Corporation
* @copyright © 2014-2015 by Mike Sharkey
* @details This file is part of CARIBOU RTOS
* CARIBOU RTOS is free software: you can redistribute it and/or modify
* it under the terms of the Beerware License Version 43.
* "THE BEER-WARE LICENSE" (Revision 43):
* Mike Sharkey <mike@pikeaero.com> wrote this file. 
* As long as you retain this notice you can do whatever you want with
* this stuff. If we meet some day, and you think this stuff is 
* worth it, you can buy me a beer in return ~ Mike Sharkey
******************************************************************************/
function getState(thread_x)
{
	var thread_current=Debug.evaluate("caribou_state.current");
	var thread_xt = Debug.evaluate("*(caribou_thread_t*)"+thread_x);
	var stateStr="";

	if ( thread_x == thread_current )
	{
		stateStr = "R";
	}
	else
	{
		if ( (thread_xt.state & 0x07) == 0 )
			stateStr = "r";
		else
		{
			stateStr = "";
			if (thread_xt.state & 0x01)
				stateStr += "S";
			if (thread_xt.state & 0x02)
				stateStr += "Y";
			if (thread_xt.state & 0x04)
				stateStr += "T";
		}
	}

	return stateStr;
}

function getregs(x)
{
  var sp = Debug.evaluate("((caribou_thread_t*)"+x+")->sp");
  if (sp & 1)
	sp += 63; // FP has been saved
  if (sp & 2)
	sp += 126; // D17-D31 has been saved
  var a = new Array();
  for (i=4;i<12;i++)
	{
	  a[i] = TargetInterface.peekWord(sp); 
	  sp+=4;
	}
  for (i=0;i<4;i++)
	{
	  a[i] = TargetInterface.peekWord(sp);	
	  sp+=4;
	}
  a[12] = TargetInterface.peekWord(sp); 
  sp+=4;
  a[14] = TargetInterface.peekWord(sp);	
  sp+=4;
  a[15] = TargetInterface.peekWord(sp); 
  sp+=4;
  a[16] = TargetInterface.peekWord(sp); 
  sp+=4;
  a[13] = sp;
  return a;
}

function gettls(x)
{
  if (x==null)
	return "ctl_task_executing->thread_local_storage";
  else
	return "((CTL_TASK_t*)"+x+")->thread_local_storage";
}

/**
 * @brief The last argument to Threads.add(...) is used to call this function, and should be a thread
 * descriptor.
 * @return An 16 element array beginning with register R0 contents.
 */
function getregs(x)
{
	var a = new Array();

	//var sp = Debug.evaluate("((caribou_thread_t*)"+x+")->sp");
	var sp = x.sp;

	if (sp & 1)									/* VFP has been stacked? */
		sp += 63;

	if (sp & 2)									/* Cortex A??? */
		sp += 126; // D17-D31 has been saved

	sp+=4;

	for (i=4;i<=11;i++)							/* R4 - R11 */
	{
		a[i] = TargetInterface.peekWord(sp); 
		sp+=4;
	}

	for (i=0;i<=3;i++)							/* R0 - R3 */
	{
		a[i] = TargetInterface.peekWord(sp);  
		sp+=4;
	}
	
	a[12] = TargetInterface.peekWord(sp);		/* R12 */
	sp+=4;

	
	a[14] = TargetInterface.peekWord(sp);		/* R14 */
	sp+=4;

	a[15] = TargetInterface.peekWord(sp);		/* R15 */
	sp+=4;

	a[16] = TargetInterface.peekWord(sp);		/* xPSR */
	sp+=4;
	
	a[13] = sp;									/* R13 (SP) */
	
	return a;
}

function program_counter(x)
{
	var a = getregs(x);
	return "0x"+(a[15].toString(16));
}

function init()
{
  Threads.setColumns(	"Name", 
						"Priority", 
						"Lock", 
						"State", 
						"Time", 
						"Pgm Ctr",
						"Stack Base", 
						"Stack Top", 
						"Stack Ptr", 
						"Stk Sz", 
						"Stk Use");
  Threads.setSortByNumber("Name");
}

function update() 
{
	var thread_x=Debug.evaluate("caribou_state.queue");
	if ( thread_x )
	{
		var thread_current=Debug.evaluate("caribou_state.current");
		Threads.clear();
		Threads.newqueue("Task List");
		while ( thread_x )
		{
			var thread_xt = Debug.evaluate("*(caribou_thread_t*)"+thread_x);
			var stack_free=thread_xt.stack_top - thread_xt.sp;
			var stack_size;
			var stack_base_hex;
			var stack_top_hex;
			var stack_ptr_hex;
			if ( thread_xt.stack_usage )
			{
				stack_use = thread_xt.stack_top - thread_xt.stack_usage;
				stack_size = thread_xt.stack_top - thread_xt.stack_base;
			}
			stack_base_hex = ("0x" + thread_xt.stack_base.toString(16));
			stack_top_hex = ("0x" + thread_xt.stack_top.toString(16));
			stack_ptr_hex = ("0x" + thread_xt.sp.toString(16));
			Threads.add(thread_xt.name,
						thread_xt.prio,
						thread_xt.lock,
						getState(thread_x),
						thread_xt.runtime/1000.0,
						program_counter(thread_xt),
                        stack_base_hex,
						stack_top_hex,
						stack_ptr_hex,
						stack_size,
						stack_use,
						getregs(thread_xt));
			thread_x = thread_xt.next;
		}
	}
}
