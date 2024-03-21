#include "ModbusPacketTask.h"
#include "cmsis_os.h"
#include "modbus.h"
#include "fm25v02.h"
#include "m95.h"

extern osSemaphoreId ModbusPacketReceiveHandle;
extern osTimerId Ring_Center_TimerHandle;
extern osMutexId Fm25v02MutexHandle;
extern osMutexId UartMutexHandle;
extern osThreadId M95TaskHandle;

extern uint32_t crc_temp;
extern uint8_t modbus_buffer[10][6000];
extern uint8_t modbus_packet_number1;

uint8_t buf_out[256];
uint8_t buf_out1[256];
uint16_t modbus_size;
uint16_t modbus_address;

extern bootloader_register_struct bootloader_registers;

extern volatile uint16_t modem_transmit_delay;
extern volatile uint8_t modem_transmit_delay_state;



void ThreadModbusPacketTask(void const * argument)
{
	uint8_t temp_reg_h;
	uint8_t temp_reg_l;

	osSemaphoreWait(ModbusPacketReceiveHandle, osWaitForever); // обнуляем семафор, при создании семафора его значение равно 1



	for(;;)
	{
		osSemaphoreWait(ModbusPacketReceiveHandle, osWaitForever);

		switch(modbus_buffer[modbus_packet_number1][1]) // проверяем тип поступившей команды MODBUS и формируем соответствующий ответ
		{
			case(0x03): // чтение регистра

				modbus_address = (((((uint16_t)modbus_buffer[modbus_packet_number1][2])<<8)&0xFF00)|(((uint16_t)modbus_buffer[modbus_packet_number1][3])&0xFF)); // считаем адрес регистра для чтения
				modbus_size = (((((uint16_t)modbus_buffer[modbus_packet_number1][4])<<8)&0xFF00)|(((uint16_t)modbus_buffer[modbus_packet_number1][5])&0xFF)); //  считаем количество регистров для чтения

				// обработка пакета чтения 16-битного регистра modbus

				osMutexWait(Fm25v02MutexHandle, osWaitForever);
				fm25v02_fast_read( 2*modbus_address , &buf_out[0] , 2*modbus_size); // читаем из памяти необходимое количество регистров
				osMutexRelease(Fm25v02MutexHandle);

				buf_out1[0] = 0x01;
				buf_out1[1] = 0x03;
				buf_out1[2] = 2*modbus_size;
				for(uint8_t i=0; i<modbus_size; i++)
				{
					buf_out1[2*i+3] = buf_out[2*i];
					buf_out1[2*i+4] = buf_out[2*i+1];
				}
				crc_temp = CRC16(&buf_out1[0], 3+2*modbus_size);
				buf_out1[2*modbus_size+3] = (uint8_t)(crc_temp&0x00FF);
				buf_out1[2*modbus_size+4] = (uint8_t)((crc_temp>>8)&0x00FF);

				osMutexWait(UartMutexHandle, osWaitForever);
				//if( AT_QISEND(&buf_out1[0], 2*modbus_size+5) != AT_OK )
				if( AT_CIPSEND(&buf_out1[0], 2*modbus_size+5) != AT_OK )
				{

				}
				osMutexRelease(UartMutexHandle);

				modem_transmit_delay_state = 0;
				modem_transmit_delay = 0;
				osThreadResume(M95TaskHandle);

				if( modbus_address == VERSION_REG ) // если запрашивается адрес версии прошивки обновляем таймер перезагрузки
				{
					osTimerStart(Ring_Center_TimerHandle, 90000); // поправил время для таймера перезагрузки
				}


			break;

			case(0x10): // запись нескольких регистров

				modbus_address = (((((uint16_t)modbus_buffer[modbus_packet_number1][2])<<8)&0xFF00)|(((uint16_t)modbus_buffer[modbus_packet_number1][3])&0xFF)); // считаем адрес регистра для записи
				modbus_size = (((((uint16_t)modbus_buffer[modbus_packet_number1][4])<<8)&0xFF00)|(((uint16_t)modbus_buffer[modbus_packet_number1][5])&0xFF)); //  считаем количество регистров для чтения

				if( !( (modbus_address>=0x1000) && (modbus_address<=0x108F) ) && !( (modbus_address<0x1000) && (modbus_address+modbus_size>0x1000) ) ) // модбас адресс не должен находиться в области статусных регистров, а также запись не должна затрагивать статусные регистры
				{

					// обработка пакета записи 16-битного регистра modbus

					//for(uint8_t a=0; a<(modbus_buffer[modbus_packet_number1][6])/2; a++)
					for(uint16_t a=0; a<modbus_size; a++)
					{
						osMutexWait(Fm25v02MutexHandle, osWaitForever);
						fm25v02_fast_write( (2*modbus_address+2*a), &modbus_buffer[modbus_packet_number1][7+a*2], 1 );
						fm25v02_fast_write( (2*modbus_address+2*a+1), &modbus_buffer[modbus_packet_number1][8+a*2], 1 );
						osMutexRelease(Fm25v02MutexHandle);
					}

					buf_out1[0] = 0x01;
					buf_out1[1] = 0x10;
					buf_out1[2] = modbus_buffer[modbus_packet_number1][2];
					buf_out1[3] = modbus_buffer[modbus_packet_number1][3];
					buf_out1[4] = modbus_buffer[modbus_packet_number1][4];
					buf_out1[5] = modbus_buffer[modbus_packet_number1][5];

					crc_temp = CRC16(&buf_out1[0], 6);

					buf_out1[6] = (uint8_t)(crc_temp&0x00FF);
					buf_out1[7] = (uint8_t)((crc_temp>>8)&0x00FF);


					osMutexWait(Fm25v02MutexHandle, osWaitForever); // читаем значения регистров

					fm25v02_read(2*CLEAR_PAGE_ON_REG, &temp_reg_h);
					fm25v02_read(2*CLEAR_PAGE_ON_REG+1, &temp_reg_l);
					bootloader_registers.clear_page_on_reg = (((uint16_t)temp_reg_h)<<8)|temp_reg_l;

					fm25v02_read(2*WRITE_ARRAY_REG, &temp_reg_h);
					fm25v02_read(2*WRITE_ARRAY_REG+1, &temp_reg_l);
					bootloader_registers.write_array_reg = (((uint16_t)temp_reg_h)<<8)|temp_reg_l;

					fm25v02_read(2*READ_ARRAY_REG, &temp_reg_h);
					fm25v02_read(2*READ_ARRAY_REG+1, &temp_reg_l);
					bootloader_registers.read_array_reg = (((uint16_t)temp_reg_h)<<8)|temp_reg_l;

					osMutexRelease(Fm25v02MutexHandle);

					while( (bootloader_registers.clear_page_on_reg == 0x0001) || (bootloader_registers.write_array_reg == 0x0001) || (bootloader_registers.read_array_reg == 0x0001) ) // вычитываем регистры пока не будет выполнено стирание страниц, запись или чтение данных из памяти контроллера
					{

						osMutexWait(Fm25v02MutexHandle, osWaitForever);

						fm25v02_read(2*CLEAR_PAGE_ON_REG, &temp_reg_h);
						fm25v02_read(2*CLEAR_PAGE_ON_REG+1, &temp_reg_l);
						bootloader_registers.clear_page_on_reg = (((uint16_t)temp_reg_h)<<8)|temp_reg_l;

						fm25v02_read(2*WRITE_ARRAY_REG, &temp_reg_h);
						fm25v02_read(2*WRITE_ARRAY_REG+1, &temp_reg_l);
						bootloader_registers.write_array_reg = (((uint16_t)temp_reg_h)<<8)|temp_reg_l;

						fm25v02_read(2*READ_ARRAY_REG, &temp_reg_h);
						fm25v02_read(2*READ_ARRAY_REG+1, &temp_reg_l);
						bootloader_registers.read_array_reg = (((uint16_t)temp_reg_h)<<8)|temp_reg_l;

						osMutexRelease(Fm25v02MutexHandle);

						osDelay(1);

					}


					osMutexWait(UartMutexHandle, osWaitForever);
					//if( AT_QISEND(&buf_out1[0], 8) != AT_OK )
					if( AT_CIPSEND(&buf_out1[0], 8) != AT_OK )
					{

					}
					osMutexRelease(UartMutexHandle);

					modem_transmit_delay_state = 0;
					modem_transmit_delay = 0;
					osThreadResume(M95TaskHandle);

				}

				osTimerStart(Ring_Center_TimerHandle, 90000); // поправил время для таймера перезагрузки


			break;
		}


		osDelay(1);
	}
}
