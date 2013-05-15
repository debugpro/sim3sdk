/*
             LUFA Library
     Copyright (C) Dean Camera, 2013.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2013  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the Mass Storage demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#define  INCLUDE_FROM_MASSSTORAGE_C
#include <circular_buffer.h>
#include "MassStorage.h"

/** Structure to hold the latest Command Block Wrapper issued by the host, containing a SCSI command to execute. */
MS_CommandBlockWrapper_t  CommandBlock;

/** Structure to hold the latest Command Status Wrapper to return to the host, containing the status of the last issued command. */
MS_CommandStatusWrapper_t CommandStatus = { .Signature = MS_CSW_SIGNATURE };

/** Flag to asynchronously abort any in-progress data transfers upon the reception of a mass storage reset command. */
volatile bool IsMassStoreReset = false;


/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
void app_main(void)
{
    USB_Init(USB_DEVICE_OPT_FULLSPEED);

	for (;;)
	{
		MassStorage_Task();
	}
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs. */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
//	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);

	/* Reset the MSReset flag upon connection */
	IsMassStoreReset = false;
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the Mass Storage management task.
 */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
//	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the Mass Storage management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup Mass Storage Data Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(MASS_STORAGE_IN_EPADDR,  EP_TYPE_BULK, MASS_STORAGE_IO_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(MASS_STORAGE_OUT_EPADDR, EP_TYPE_BULK, MASS_STORAGE_IO_EPSIZE, 1);

	/* Indicate endpoint configuration success or failure */
//	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	/* Process UFI specific control requests */
	switch (USB_ControlRequest.bRequest)
	{
		case MS_REQ_MassStorageReset:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				/* Indicate that the current transfer should be aborted */
				IsMassStoreReset = true;
			}

			break;
		case MS_REQ_GetMaxLUN:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Indicate to the host the number of supported LUNs (virtual disks) on the device */
				Endpoint_Write_8(TOTAL_LUNS - 1);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
	}
}
uint32_t msc_state;
extern uint8_t sec_buf[];
void EVENT_USB_common_request(void)
{
	switch(msc_state)
	{
		case MSC_READY:
			if(ReadInCommandBlock())
			{
				/* Check direction of command, select Data IN endpoint if data is from the device */
				if (CommandBlock.Flags & MS_COMMAND_DIR_DATA_IN)
				{
				  Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
				}
				/* Decode the received SCSI command, set returned status code */
				CommandStatus.Status = SCSI_DecodeSCSICommand() ? MS_SCSI_COMMAND_Pass : MS_SCSI_COMMAND_Fail;
				/* Load in the CBW tag into the CSW to link them together */
				CommandStatus.Tag = CommandBlock.Tag;
				/* Load in the data residue counter into the CSW */
				CommandStatus.DataTransferResidue = CommandBlock.DataTransferLength;
				/* Stall the selected data pipe if command failed (if data is still to be transferred) */
				if ((CommandStatus.Status == MS_SCSI_COMMAND_Fail) && (CommandStatus.DataTransferResidue))
				{
				    Endpoint_StallTransaction();
				    msc_state = MSC_ERROR;
				}
				if(msc_state == MSC_READY)
				{
					msc_state = MSC_CSW_SEND;
				}
			}
			break;
		case MSC_DATA_IN:
			break;
		case MSC_DATA_OUT:
            Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);
            Endpoint_Read_Stream_LE(&sec_buf[offset_within_block],64,NULL);
            offset_within_block += 64;
            if(offset_within_block == 512)
            {
            	total_blocks--;
            }
			if(total_blocks == 0)
			{
				msc_state = MSC_CSW_SEND;
			}
			break;
		case MSC_CSW_SEND:
			/* Return command status block to the host */
			ReturnCommandStatus();
			msc_state = MSC_READY;
			break;
		case MSC_ERROR:
			Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);
			if (!Endpoint_IsStalled())
			{
				Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
				if(!Endpoint_IsStalled())
				{
					msc_state = MSC_CSW_SEND;
				}
			}
			break;
		default:
			break;

	}
}
/** Task to manage the Mass Storage interface, reading in Command Block Wrappers from the host, processing the SCSI commands they
 *  contain, and returning Command Status Wrappers back to the host to indicate the success or failure of the last issued command.
 */
void MassStorage_Task(void)
{
    uint32_t tmp = 0;
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;
#if 0
	/* Process sent command block from the host if one has been sent */
	if (ReadInCommandBlock())
	{
		/* Check direction of command, select Data IN endpoint if data is from the device */
		if (CommandBlock.Flags & MS_COMMAND_DIR_DATA_IN)
		{
		  Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
		}
		/* Decode the received SCSI command, set returned status code */
		CommandStatus.Status = SCSI_DecodeSCSICommand() ? MS_SCSI_COMMAND_Pass : MS_SCSI_COMMAND_Fail;
		/* Load in the CBW tag into the CSW to link them together */
		CommandStatus.Tag = CommandBlock.Tag;
		/* Load in the data residue counter into the CSW */
		CommandStatus.DataTransferResidue = CommandBlock.DataTransferLength;
		/* Stall the selected data pipe if command failed (if data is still to be transferred) */
		if ((CommandStatus.Status == MS_SCSI_COMMAND_Fail) && (CommandStatus.DataTransferResidue))
		{
		    Endpoint_StallTransaction();
		}
		/* Return command status block to the host */
		ReturnCommandStatus();
	}
#endif
	/* Check if a Mass Storage Reset occurred */
	if (IsMassStoreReset)
	{
		/* Reset the data endpoint banks */
//		Endpoint_ResetEndpoint(MASS_STORAGE_OUT_EPADDR);
//		Endpoint_ResetEndpoint(MASS_STORAGE_IN_EPADDR);

		Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);
		Endpoint_ClearStall();
		Endpoint_ResetDataToggle();
		Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
		Endpoint_ClearStall();
		Endpoint_ResetDataToggle();

		/* Clear the abort transfer flag */
		IsMassStoreReset = false;
	}
}

/** Function to read in a command block from the host, via the bulk data OUT endpoint. This function reads in the next command block
 *  if one has been issued, and performs validation to ensure that the block command is valid.
 *
 *  \return Boolean \c true if a valid command block has been read in from the endpoint, \c false otherwise
 */
static bool ReadInCommandBlock(void)
{
#if 1
	uint16_t BytesTransferred;

	/* Select the Data Out endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);

	/* Abort if no command has been sent from the host */
	if (!(Endpoint_IsOUTReceived()))
	  return false;
	/* Read in command block header */
	BytesTransferred = 0;
	if(Endpoint_BytesInEndpoint() != sizeof(CommandBlock))
	{
		BytesTransferred = 1;
	}
	while (Endpoint_Read_Stream_LE(&CommandBlock, (sizeof(CommandBlock)),
	                               &BytesTransferred) == ENDPOINT_RWSTREAM_IncompleteTransfer)
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return false;
	}

	/* Verify the command block - abort if invalid */
	if ((CommandBlock.Signature         != MS_CBW_SIGNATURE) ||
	    (CommandBlock.LUN               >= TOTAL_LUNS)       ||
		(CommandBlock.Flags              & 0x1F)             ||
		(CommandBlock.SCSICommandLength == 0)                ||
		(CommandBlock.SCSICommandLength >  sizeof(CommandBlock.SCSICommandData)))
	{
		/* Stall both data pipes until reset by host */
		Endpoint_StallTransaction();
		Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
		Endpoint_StallTransaction();

		return false;
	}
	/* Finalize the stream transfer to send the last packet */
	Endpoint_ClearOUT();

	return true;
#else
	uint16_t BytesTransferred;

	/* Select the Data Out endpoint */
	circular_buffer_pools_t *cb_out = circular_buffer_pointer(MASS_STORAGE_OUT_EPADDR);

	if(circular_buffer_count(cb_out) < sizeof(CommandBlock))
	    return false;

	/* Read in command block header */
	BytesTransferred = 0;
	circular_buffer_read(cb_out,&CommandBlock,sizeof(CommandBlock),1);
	/* Verify the command block - abort if invalid */
	if ((CommandBlock.Signature         != MS_CBW_SIGNATURE) ||
	    (CommandBlock.LUN               >= TOTAL_LUNS)       ||
		(CommandBlock.Flags              & 0x1F)             ||
		(CommandBlock.SCSICommandLength == 0)                ||
		(CommandBlock.SCSICommandLength >  sizeof(CommandBlock.SCSICommandData)))
	{
		/* Stall both data pipes until reset by host */
        Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);
		Endpoint_StallTransaction();
		Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
		Endpoint_StallTransaction();

		return false;
	}
	return true;
#endif
}

/** Returns the filled Command Status Wrapper back to the host via the bulk data IN endpoint, waiting for the host to clear any
 *  stalled data endpoints as needed.
 */
static void ReturnCommandStatus(void)
{
	uint16_t BytesTransferred;
#if 0
	/* Select the Data Out endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);

	/* While data pipe is stalled, wait until the host issues a control request to clear the stall */
	while (Endpoint_IsStalled())
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return;
	}

	/* Select the Data In endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);

	/* While data pipe is stalled, wait until the host issues a control request to clear the stall */
	while (Endpoint_IsStalled())
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return;
	}
#endif
	/* Write the CSW to the endpoint */
	BytesTransferred = 0;
    Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
	Endpoint_Write_Stream_LE(&CommandStatus, sizeof(CommandStatus), &BytesTransferred);
	/* Finalize the stream transfer to send the last packet */
	Endpoint_ClearIN();
}
