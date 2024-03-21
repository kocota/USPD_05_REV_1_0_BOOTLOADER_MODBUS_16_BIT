#include "cmsis_os.h"
#include "modbus.h"

extern osThreadId M95TaskHandle;
extern osMessageQId ModbusQueueHandle;
extern osSemaphoreId ModbusPacketReceiveHandle;

extern uint8_t modbus_buffer[10][6000];

osEvent ModbusEvent;

uint32_t crc_temp;
uint8_t modbus_packet_number = 0;
uint8_t modbus_packet_number1 = 0;

uint16_t i_m;
volatile uint16_t modem_transmit_delay = 0;
volatile uint8_t modem_transmit_delay_state = 0;


void ThreadModbusTask(void const * argument)
{
	uint16_t i=0;
	uint16_t i_max;


	for(;;)
	{
		ModbusEvent = osMessageGet(ModbusQueueHandle, osWaitForever); // ожидаем сообщение
		if(ModbusEvent.status == osEventMessage) // если сообщение пришло
		{

			switch(ModbusEvent.value.v) // проверяем полученное значение из очереди
			{
				case(0x01):
					if(i==0)
					{
						modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
						i++;
						//modem_transmit_delay_state = 1;
						//osThreadSuspend(M95TaskHandle);
					}
					else if(i==1)
					{
						i = 0;
						i_max = 0;
					}
					else if(i>1)
					{
						if( (i==6) && (modbus_buffer[modbus_packet_number][1] == 0x10) )
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							//i_max = 9 + (uint8_t)ModbusEvent.value.v;
							i_max = 9 + 2*( (((uint16_t)modbus_buffer[modbus_packet_number][4])<<8) | ((uint16_t)modbus_buffer[modbus_packet_number][5]) );
							i_m = i_max;
							i++;
						}
						else
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							i++;
						}
					}

				break;

				case(0x03):
					if(i==0)
					{
						i = 0;
						i_max = 0;
					}
					else if(i==1)
					{
						modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
						i++;
						i_max = 8;
						modem_transmit_delay_state = 1;
						osThreadSuspend(M95TaskHandle);
					}
					else if(i>1)
					{
						if( (i==6) && (modbus_buffer[modbus_packet_number][1] == 0x10) )
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							//i_max = 9 + (uint8_t)ModbusEvent.value.v;
							i_max = 9 + 2*( (((uint16_t)modbus_buffer[modbus_packet_number][4])<<8) | ((uint16_t)modbus_buffer[modbus_packet_number][5]) );
							i_m = i_max;
							i++;
						}
						else
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							i++;
						}
					}

				break;

				case(0x06):
					if(i==0)
					{
						i = 0;
						i_max = 0;
					}
					else if(i==1)
					{
						modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
						i++;
						i_max = 8;
					}
					if(i>1)
					{
						if( (i==6) && (modbus_buffer[modbus_packet_number][1] == 0x10) )
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							//i_max = 9 + (uint8_t)ModbusEvent.value.v;
							i_max = 9 + 2*( (((uint16_t)modbus_buffer[modbus_packet_number][4])<<8) | ((uint16_t)modbus_buffer[modbus_packet_number][5]) );
							i_m = i_max;
							i++;
						}
						else
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							i++;
						}
					}

				break;

				case(0x10):
					if(i==0)
					{
						i = 0;
						i_max = 0;
					}
					else if(i==1)
					{
						modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
						i++;
						modem_transmit_delay_state = 1;
						osThreadSuspend(M95TaskHandle);
					}
					else if(i>1)
					{
						if( (i==6) && (modbus_buffer[modbus_packet_number][1] == 0x10) )
						{
								modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
								//i_max = 9 + (uint8_t)ModbusEvent.value.v;
								i_max = 9 + 2*( (((uint16_t)modbus_buffer[modbus_packet_number][4])<<8) | ((uint16_t)modbus_buffer[modbus_packet_number][5]) );
								i_m = i_max;
								i++;
						}
						else
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							i++;
						}
					}

				break;

				default:
					if(i==0)
					{
						i = 0;
						i_max = 0;
					}
					else if(i==1)
					{
						i = 0;
						i_max = 0;
					}
					else if(i>1)
					{
						if( (i==6) && (modbus_buffer[modbus_packet_number][1] == 0x10) )
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							//i_max = 9 + (uint8_t)ModbusEvent.value.v;
							i_max = 9 + 2*( (((uint16_t)modbus_buffer[modbus_packet_number][4])<<8) | ((uint16_t)modbus_buffer[modbus_packet_number][5]) );
							i_m = i_max;
							i++;
						}
						else
						{
							modbus_buffer[modbus_packet_number][i] = ModbusEvent.value.v;
							i++;
						}
					}

				break;
			}
			if( (i >= i_max) && (i != 0) && (i_max != 0) ) // если число принятых байт соответствует длине соответствующей команды
			{
				crc_temp = CRC16(&modbus_buffer[modbus_packet_number][0], i_max-2); // считаем контрольную сумму принятого пакета
				if( ( ((crc_temp>>8)&0x00FF) == modbus_buffer[modbus_packet_number][i_max-1] ) && ( (crc_temp&0x00FF) == modbus_buffer[modbus_packet_number][i_max-2]) ) // проверяем контрольную сумму принятого пакета
				{
					modbus_packet_number1 = modbus_packet_number;
					modbus_packet_number++;
					if( modbus_packet_number >= 10)
					{
						modbus_packet_number = 0;
					}

					osSemaphoreRelease(ModbusPacketReceiveHandle);
				}

				i = 0;     // обнуляем значение текущего принятого байта
				i_max = 0; // обнуляем максимальное значение принятого байта

				//modem_transmit_delay_state = 0;
				//modem_transmit_delay = 0;
				//osThreadResume(M95TaskHandle);

			}
		}

		if(modem_transmit_delay_state == 1)
		{
			modem_transmit_delay++;
			if(modem_transmit_delay >= 10000)
			{
				osThreadResume(M95TaskHandle);
				modem_transmit_delay = 0;
				modem_transmit_delay_state = 0;
			}
		}


		//osDelay(1);
	}
}
