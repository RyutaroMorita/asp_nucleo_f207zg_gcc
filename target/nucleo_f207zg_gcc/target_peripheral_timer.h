/*
 * target_peripheral_timer.h
 *
 *  Created on: 2025/09/16
 *      Author: ryuta
 */

#ifndef TARGET_PERIPHERAL_TIMER_H_
#define TARGET_PERIPHERAL_TIMER_H_

#define INHNO_TIM2		(TIM2_IRQn + 16)		/* 28 */
#define INTNO_TIM2		(TIM2_IRQn + 16)		/* 28 */
#define INTATR_TIM2		(0)		/* 割込み属性	*/
#define INTPRI_TIM2		(-1)						/* 割込み優先度	*/

extern void target_peripheral_timer_initialize(intptr_t exinf);

#endif /* TARGET_PERIPHERAL_TIMER_H_ */
