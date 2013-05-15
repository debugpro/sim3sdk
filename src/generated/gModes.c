//-------------------------------------------------------------------------------
// Copyright (c) 2012 by Silicon Laboratories
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Silicon Laboratories End User
// License Agreement which accompanies this distribution, and is available at
// http://developer.silabs.com/legal/version/v10/License_Agreement_v10.htm
//
//
// Original content and implementation provided by Silicon Laboratories
//-------------------------------------------------------------------------------

//==============================================================================
// WARNING:
//
// This file is auto-generated by AppBuilder and should not be modified.
// Any hand modifications will be lost if the project is regenerated.
//==============================================================================

#include "gCLKCTRL.h"
#include "gCPU.h"
#include "gFLASHCTRL0.h"
#include "gPB.h"
#include "gUART0.h"
#include "gSPI0.h"
#include "gTIMER.h"
#include <si32_device.h>

//==============================================================================
//MODE FUNCTIONS
//==============================================================================
void enter_default_mode_from_reset(void)
{
  // Setup clock gates
  CLKCTRL_setup_default_mode_clock_gates();


  // Init FLASHCTRL when AHB frequency > 20 MHz
  FLASHCTRL0_enter_default_mode_from_reset();

  // Setup ports
  pb_enter_default_mode_from_reset();
  //gSPI0_enter_master_mode_config();
  gSPI0_enter_dma_master_mode_config();
  // Initialize clock control
  SystemCoreClock = 20000000;
  cpu_update();
  CLKCTRL_enter_default_mode_from_reset();

  // Initialize peripherals
  cpu_update();
  UART0_enter_default_mode_from_reset();
  TIMER_enter_default_mode_from_reset();
}

//---eof------------------------------------------------------------------------
