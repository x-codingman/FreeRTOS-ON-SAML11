/**
 * \file
 *
 * \brief TrustZone secure gateway veneer implementation.
 *
 * Copyright (c) 2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

#include "atmel_start.h"
#include "trustzone_veneer.h"
#include "trustzone_config.h"


/* Secure heap includes. */
#include "secure_heap.h"

/* Secure port macros. */
#include "secure_port_macros.h"
/* Secure context includes. */
#include "secure_context.h"

/* ======== Non-secure callable functions for Getting Started Example ======== */
extern int func_plus3(int x);
extern int func_minus2(int x);









/**
 * @brief CONTROL value for privileged tasks.
 *
 * Bit[0] - 0 --> Thread mode is privileged.
 * Bit[1] - 1 --> Thread mode uses PSP.
 */
#define securecontextCONTROL_VALUE_PRIVILEGED		0x02

/**
 * @brief CONTROL value for un-privileged tasks.
 *
 * Bit[0] - 1 --> Thread mode is un-privileged.
 * Bit[1] - 1 --> Thread mode uses PSP.
 */
#define securecontextCONTROL_VALUE_UNPRIVILEGED		0x03
/*-----------------------------------------------------------*/

/**
 * @brief Structure to represent secure context.
 *
 * @note Since stack grows down, pucStackStart is the highest address while
 * pucStackLimit is the first addess of the allocated memory.
 */
typedef struct SecureContext
{
	uint8_t *pucCurrentStackPointer;	/**< Current value of stack pointer (PSP). */
	uint8_t *pucStackLimit;				/**< Last location of the stack memory (PSPLIM). */
	uint8_t *pucStackStart;				/**< First location of the stack memory. */
} SecureContext_t;
/*-----------------------------------------------------------*/

secureportNON_SECURE_CALLABLE void SecureContext_Init( void )
{
	uint32_t ulIPSR;

	/* Read the Interrupt Program Status Register (IPSR) value. */
	secureportREAD_IPSR( ulIPSR );

	/* Do nothing if the processor is running in the Thread Mode. IPSR is zero
	 * when the processor is running in the Thread Mode. */
	if( ulIPSR != 0 )
	{
		/* No stack for thread mode until a task's context is loaded. */
		secureportSET_PSPLIM( securecontextNO_STACK );
		secureportSET_PSP( securecontextNO_STACK );

		#if( configENABLE_MPU == 1 )
		{
			/* Configure thread mode to use PSP and to be unprivileged. */
			secureportSET_CONTROL( securecontextCONTROL_VALUE_UNPRIVILEGED );
		}
		#else /* configENABLE_MPU */
		{
			/* Configure thread mode to use PSP and to be privileged.. */
			secureportSET_CONTROL( securecontextCONTROL_VALUE_PRIVILEGED );
		}
		#endif /* configENABLE_MPU */
	}
}
/*-----------------------------------------------------------*/

#if( configENABLE_MPU == 1 )
	secureportNON_SECURE_CALLABLE SecureContextHandle_t SecureContext_AllocateContext( uint32_t ulSecureStackSize, uint32_t ulIsTaskPrivileged )
#else /* configENABLE_MPU */
	secureportNON_SECURE_CALLABLE SecureContextHandle_t SecureContext_AllocateContext( uint32_t ulSecureStackSize )
#endif /* configENABLE_MPU */
{
	uint8_t *pucStackMemory = NULL;
	uint32_t ulIPSR;
	SecureContextHandle_t xSecureContextHandle = NULL;
	#if( configENABLE_MPU == 1 )
		uint32_t *pulCurrentStackPointer = NULL;
	#endif /* configENABLE_MPU */

	/* Read the Interrupt Program Status Register (IPSR) value. */
	secureportREAD_IPSR( ulIPSR );

	/* Do nothing if the processor is running in the Thread Mode. IPSR is zero
	 * when the processor is running in the Thread Mode. */
	if( ulIPSR != 0 )
	{
		/* Allocate the context structure. */
		xSecureContextHandle = ( SecureContextHandle_t ) pvPortMalloc( sizeof( SecureContext_t ) );

		if( xSecureContextHandle != NULL )
		{
			/* Allocate the stack space. */
			pucStackMemory = pvPortMalloc( ulSecureStackSize );

			if( pucStackMemory != NULL )
			{
				/* Since stack grows down, the starting point will be the last
				 * location. Note that this location is next to the last
				 * allocated byte because the hardware decrements the stack
				 * pointer before writing i.e. if stack pointer is 0x2, a push
				 * operation will decrement the stack pointer to 0x1 and then
				 * write at 0x1. */
				xSecureContextHandle->pucStackStart = pucStackMemory + ulSecureStackSize;

				/* The stack cannot go beyond this location. This value is
				 * programmed in the PSPLIM register on context switch.*/
				xSecureContextHandle->pucStackLimit = pucStackMemory;

				#if( configENABLE_MPU == 1 )
				{
					/* Store the correct CONTROL value for the task on the stack.
					 * This value is programmed in the CONTROL register on
					 * context switch. */
					pulCurrentStackPointer = ( uint32_t * ) xSecureContextHandle->pucStackStart;
					pulCurrentStackPointer--;
					if( ulIsTaskPrivileged )
					{
						*( pulCurrentStackPointer ) = securecontextCONTROL_VALUE_PRIVILEGED;
					}
					else
					{
						*( pulCurrentStackPointer ) = securecontextCONTROL_VALUE_UNPRIVILEGED;
					}

					/* Store the current stack pointer. This value is programmed in
					 * the PSP register on context switch. */
					xSecureContextHandle->pucCurrentStackPointer = ( uint8_t * ) pulCurrentStackPointer;
				}
				#else /* configENABLE_MPU */
				{
					/* Current SP is set to the starting of the stack. This
					 * value programmed in the PSP register on context switch. */
					xSecureContextHandle->pucCurrentStackPointer = xSecureContextHandle->pucStackStart;

				}
				#endif /* configENABLE_MPU */
			}
			else
			{
				/* Free the context to avoid memory leak and make sure to return
				 * NULL to indicate failure. */
				vPortFree( xSecureContextHandle );
				xSecureContextHandle = NULL;
			}
		}
	}

	return xSecureContextHandle;
}
/*-----------------------------------------------------------*/

secureportNON_SECURE_CALLABLE void SecureContext_FreeContext( SecureContextHandle_t xSecureContextHandle )
{
	uint32_t ulIPSR;

	/* Read the Interrupt Program Status Register (IPSR) value. */
	secureportREAD_IPSR( ulIPSR );

	/* Do nothing if the processor is running in the Thread Mode. IPSR is zero
	 * when the processor is running in the Thread Mode. */
	if( ulIPSR != 0 )
	{
		/* Ensure that valid parameters are passed. */
		secureportASSERT( xSecureContextHandle != NULL );

		/* Free the stack space. */
		vPortFree( xSecureContextHandle->pucStackLimit );

		/* Free the context itself. */
		vPortFree( xSecureContextHandle );
	}
}













/*
 * \brief Non-secure callable function 1
 */
#if defined(__ICCARM__) /* IAR EWARM */
__cmse_nonsecure_entry int nsc_func_plus3(int x)
#else /* GCC, Keil MDK with ARM Compiler 6 */
int __attribute__((cmse_nonsecure_entry)) nsc_func_plus3(int x)
#endif
{
	return func_plus3(x);
}

/*
 * \brief Non-secure callable function 2
 */
#if defined(__ICCARM__) /* IAR EWARM */
__cmse_nonsecure_entry int nsc_func_minus2(int x)
#else /* GCC, Keil MDK with ARM Compiler 6 */
int __attribute__((cmse_nonsecure_entry)) nsc_func_minus2(int x)
#endif
{
	return func_minus2(x);
}

/* ======== Non-secure callable functions defined by customer ======== */
/*
 * You may add your secure gateway veneer functions in this file
 * and the function declaration into trustzone_veneer.h.
 */
#if defined(__ICCARM__) /* IAR EWARM */
__cmse_nonsecure_entry int nsc_func_minus2(int x)
#else /* GCC, Keil MDK with ARM Compiler 6 */
int __attribute__((cmse_nonsecure_entry)) nsc_printf(char * str)
#endif
{
	return printf(str);
}