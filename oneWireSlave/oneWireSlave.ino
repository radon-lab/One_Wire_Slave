/*
  Arduino IDE 1.8.13 версия прошивки oneWireSlave 0.0.2 бета от 26.10.21
  Эмулятор slave устройства для шины one wire на attiny

  Автор Radon-lab.
*/
#include <util/delay.h>

#define US_TO_TICKS(x) (uint16_t)((F_CPU / 16e6) * x)

#define BIT_SET(value, bit) ((value) |= (0x01 << (bit)))
#define BIT_CLEAR(value, bit) ((value) &= ~(0x01 << (bit)))

#define DDR_REG DDRB
#define PIN_REG PINB
#define PORT_REG PORTB

#define ADER 0x01   //ошибка адреса
#define ADOK 0x02   //адрес прочитан

#define SEARCH_ROM 0xF0 //поиск адреса
#define MATCH_ROM 0x55  //отправка адреса
#define READ_ROM 0x33   //запрос адреса
#define SKIP_ROM 0xCC   //пропуск адреса

#define READ_DATA 0xBE  //отправка массива памяти

//пин шины oneWire PB1
#define WIRE_BIT   1 // PB1

#define WIRE_SET  (BIT_SET(PORT_REG, WIRE_BIT))
#define WIRE_CLR  (BIT_CLEAR(PORT_REG, WIRE_BIT))
#define WIRE_CHK  (PIN_REG & (0x01 << WIRE_BIT))
#define WIRE_LO   (BIT_SET(DDR_REG, WIRE_BIT))
#define WIRE_HI   (BIT_CLEAR(DDR_REG, WIRE_BIT))

#define WIRE_INIT  WIRE_CLR; WIRE_HI

uint8_t wireDataBuf[9] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99}; //буфер шины oneWire
uint8_t wireAddrBuf[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}; //буфер адреса шины oneWire

int main(void) {
  cli(); //запрещаем прерывания глобально

  WIRE_INIT; //инициализация датчика температуры

  MCUCR |= (0x01 << ISC01); //настроили маску прерываний для PB1 шина oneWire
  //--------------------------------------------------------------------------------------
  for (;;) {
    if (GIFR & (0x01 << INTF0)) readOneWire(); //сигнал протокола oneWire
  }
  return 0;
}
//------------------------------Эмуляция шины 1wire---------------------------------------
void readOneWire(void) //эмуляция шины 1wire
{
  static uint8_t addrReg; //флаг работы с адресом устройства

  uint16_t TIMER = 0; //сбросили таймер
  GIFR |= (0x01 << INTF0); //сбросили флаг прерывания пина PB1

  while (!WIRE_CHK) if (++TIMER > US_TO_TICKS(2000)) return; //ждем окончания сигнала сброса
  if (TIMER < US_TO_TICKS(400)) return; //если сигнал сброса слишком короткий
  if (addrReg & (0x01 << ADER)) { //если ошибка чтения адреса
    addrReg = 0; //сбрасываем регистр адреса
    return; //выходим
  }

  _delay_us(2); //ждем
  WIRE_LO; //установили низкий уровень
  _delay_us(120); //ждем
  GIFR |= (0x01 << INTF0); //сбросили флаг прерывания пина PB1
  WIRE_HI; //установили высокий уровень
  _delay_us(2); //ждем

  if (!addrReg) { //если сетевой протокол не пройден
    switch (oneWireRead()) { //читаем байт сетевого протокола
      case READ_ROM: //комманда отправить адрес
        for (uint8_t i = 0; i < sizeof(wireAddrBuf); i++) if (oneWireWrite(wireAddrBuf[i])) return; //отправка на шину 1wire
        return; //выходим
      case MATCH_ROM: //комманда сравнить адрес
        addrReg = (0x01 << ADER); //устанавливаем флаг ошибки чтения адреса
        for (uint8_t i = 0; i < sizeof(wireAddrBuf); i++) if (oneWireRead() != wireAddrBuf[i]) return; //отправка на шину 1wire
        addrReg = (0x01 << ADOK); //устанавливаем флаг успешного чтения адреса
        return; //выходим
      case SEARCH_ROM: //комманда поиска адреса
        for (uint8_t i = 0; i < 64; i++) {
          boolean addrBit = wireAddrBuf[i >> 3] & (0x01 << (i % 8)); //находим нужный бит адреса
          oneWireWriteBit(addrBit); //отправляем прямой бит
          oneWireWriteBit(!addrBit); //отправляем инверсный бит
          if (oneWireReadBit() != addrBit) return; //отправка на шину 1wire
        }
        return; //выходим
      case SKIP_ROM: break; //пропуск адресации
    }
  }
  else addrReg = 0; //сбрасываем регистр адреса

  switch (oneWireRead()) { //читаем байт команды
    case READ_DATA: //комманда отправить температуру
      for (uint8_t i = 0; i < sizeof(wireDataBuf); i++) if (oneWireWrite(wireDataBuf[i])) return; //отправка на шину 1wire
      break;
  }
}
//-----------------------------------Отправка на шину 1wire----------------------------------------
boolean oneWireWrite(uint8_t data) //отправка на шину 1wire
{
  uint16_t TIMER = 0; //сбросили таймер
  for (uint8_t i = 0; i < 8;) { //отправляем 8 бит
    if (++TIMER > US_TO_TICKS(2000)) return 1; //ждем флага прерывания
    if (GIFR & (0x01 << INTF0)) { //если был спад
      TIMER = 0; //сбросили таймер
      if ((data >> i) & 0x01) WIRE_HI; //передаем 1
      else WIRE_LO; //передаем 0
      while (++TIMER < US_TO_TICKS(30)); //ждем
      WIRE_HI; //освобождаем линию
      GIFR |= (0x01 << INTF0); //сбросили флаг прерывания пина PB1
      i++; //сместили бит передачи
    }
  }
  return 0;
}
//--------------------------------------Чтение шины 1wire------------------------------------------
uint8_t oneWireRead(void) //чтение шины 1wire
{
  uint8_t data = 0; //временный буфер приема
  uint16_t TIMER = 0; //сбросили таймер
  for (uint8_t i = 0; i < 8;) { //читаем 8 бит
    if (++TIMER > US_TO_TICKS(2000)) return 0; //ждем флага прерывания
    if (GIFR & (0x01 << INTF0)) { //если был спад
      TIMER = 0; //сбросили таймер
      GIFR |= (0x01 << INTF0); //сбросили флаг прерывания пина PB1
      while (!WIRE_CHK) if (++TIMER > US_TO_TICKS(2000)) return 0; //ждем флага прерывания
      if (TIMER < US_TO_TICKS(30)) data |= 0x01 << i; //установли единицу
      i++; //сместили бит чтения
    }
  }
  return data; //возвращаем прочитаный байт
}
//-----------------------------------Отправка бита на шину 1wire----------------------------------------
void oneWireWriteBit(uint8_t data) //отправка бита на шину 1wire
{
  uint16_t TIMER = 0; //сбросили таймер
  while (!(GIFR & (0x01 << INTF0))) if (++TIMER > US_TO_TICKS(2000)) return; //ждем флага прерывания
  TIMER = 0; //сбросили таймер
  if (data) WIRE_HI; //передаем 1
  else WIRE_LO; //передаем 0
  while (++TIMER < US_TO_TICKS(30)); //ждем
  WIRE_HI; //освобождаем линию
  GIFR |= (0x01 << INTF0); //сбросили флаг прерывания пина PB1
}
//--------------------------------------Чтение бита шины 1wire------------------------------------------
uint8_t oneWireReadBit(void) //чтение бита шины 1wire
{
  uint16_t TIMER = 0; //сбросили таймер
  while (!(GIFR & (0x01 << INTF0))) if (++TIMER > US_TO_TICKS(2000)) return 0; //ждем флага прерывания
  TIMER = 0; //сбросили таймер
  GIFR |= (0x01 << INTF0); //сбросили флаг прерывания пина PB1
  while (!WIRE_CHK) if (++TIMER > US_TO_TICKS(2000)) return 0; //ждем флага прерывания
  if (TIMER < US_TO_TICKS(30)) return 1; //возвращаем единицу
  return 0; //возвращаем ноль
}
