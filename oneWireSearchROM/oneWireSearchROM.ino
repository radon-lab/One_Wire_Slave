#include <util/delay.h>

#define DDR_REG(portx)  (*(&portx - 1))
#define PIN_REG(portx)  (*(&portx - 2))
#define BIT_READ(value, bit) (((value) >> (bit)) & 0x01)
#define BIT_SET(value, bit) ((value) |= (0x01 << (bit)))
#define BIT_CLEAR(value, bit) ((value) &= ~(0x01 << (bit)))
#define BIT_WRITE(value, bit, bitvalue) (bitvalue ? BIT_SET(value, bit) : BIT_CLEAR(value, bit))

//пин шины 1wire D1
#define WIRE_BIT   1 // D1
#define WIRE_PORT  PORTD

#define WIRE_SET   (BIT_SET(WIRE_PORT, WIRE_BIT))
#define WIRE_CLR   (BIT_CLEAR(WIRE_PORT, WIRE_BIT))
#define WIRE_CHK   (BIT_READ(PIN_REG(WIRE_PORT), WIRE_BIT))
#define WIRE_LO    (BIT_SET((DDR_REG(WIRE_PORT)), WIRE_BIT))
#define WIRE_HI    (BIT_CLEAR((DDR_REG(WIRE_PORT)), WIRE_BIT))

#define WIRE_INIT  WIRE_CLR; WIRE_HI

int main(void) {
  WIRE_INIT;
  for (;;) {
    oneWireSearch();
  }
  return 0;
}

//-----------------------------------Поиск устройств на шине--------------------------------------------
uint16_t oneWireSearch(void) {
  uint8_t address[] = {0, 0, 0, 0, 0, 0, 0, 0}; //последний адрес устройства на линии
  uint8_t lastWrongBit = 65; //последний конфликтный бит адреса
  for (uint16_t num = 0; num < 1000; num++) { //лимит поиска адресов
    if (!oneWireSearchAddr(address, &lastWrongBit)) return num; //если устройств на шине больше нет возвращаем 0
  }
  return 0; //возвращаем 0 при превышении поиска адресов
}

//-----------------------------------Поиск нового адреса на шине--------------------------------------------
uint8_t oneWireSearchAddr(uint8_t* data, uint8_t* lastWrongBit) {
  if (!*lastWrongBit) return 0; //выходим если конфликтных ситуаций больше нету
  if (!oneWireReset()) return 0; //выходим если нету устройств на шине

  uint8_t lastByte = *data; //предыдущий байт
  uint8_t newByte = 0; //новый байт

  uint8_t posByte = 0; //номер байта адреса
  uint8_t posBit = 1; //номер бита адреса
  uint8_t newWrongBit = 0; //новый номер конфликтного бита

  oneWireWrite(0xF0); //отправляем команду поиска ROM

  while (1) {
    switch (oneWireReadBit() << 1 | oneWireReadBit()) { //считываем прямой и инверсный бит
      case 0x00:
        if (posBit < *lastWrongBit) { //если ещё не дошли до последнего конфликтного бита
          if (lastByte & 0x01) newByte |= 0x80; //тогда копируем значение бита из прошлого адреса
          else newWrongBit = posBit; //если ноль то записываем новое конфликтный бит
        }
        else if (posBit == *lastWrongBit) newByte |= 0x80; //если дошли до конфликтного бита то записываем 1
        else newWrongBit = posBit; //иначе записываем новый конфликтный бит
        break;
      case 0x02: newByte |= 0x80; break; //устанавливаем 1
      case 0x03: return 0; //выходим если нет ответа на шине
    }

    oneWireWriteBit(newByte & 0x80); //отправляем прямой бит обратно

    if (++posByte >= 8) { //если считали 8 бит
      *data++ = newByte; //записываем байт адреса
      newByte = 0; //сбрасываем буфер нового байта
      lastByte = *data; //записываем байт адреса в буфер предыдущего байта
      posByte = 0; //сбрасываем позицию байта
    }
    else {
      lastByte >>= 1; //двигаем предыдущий байт
      newByte >>= 1; //двигаем новый байт
    }
    if (posBit++ >= 64) break; //если считали все биты то выходим
  }
  *lastWrongBit = newWrongBit; //запоминаем последний конфликтный бит
  return 1; //возвращаем статус успешного чтения адреса
}
//-----------------------------------Сигнал сброса шины--------------------------------------------
boolean oneWireReset(void)
{
  WIRE_LO;
  _delay_us(520);
  WIRE_HI;
  _delay_us(2);
  for (uint8_t c = 80; c; c--) {
    if (!WIRE_CHK) {
      for (uint8_t i = 200; !WIRE_CHK && i; i--) _delay_us(1);
      return 0;
    }
    _delay_us(1);
  }
  return 1;
}
//----------------------------------Отправка данных в шину-----------------------------------------
void oneWireWrite(uint8_t data)
{
  for (uint8_t i = 0; i < 8; i++) {
    if ((data >> i) & 0x01) {
      WIRE_LO;
      _delay_us(5);
      WIRE_HI;
      _delay_us(60);
    }
    else {
      WIRE_LO;
      _delay_us(60);
      WIRE_HI;
      _delay_us(5);
    }
  }
}
//----------------------------------Чтение данных с шины--------------------------------------------
uint8_t oneWireRead(void)
{
  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    WIRE_LO;
    _delay_us(2);
    WIRE_HI;
    _delay_us(8);
    if (WIRE_CHK) data |= (0x01 << i);
    _delay_us(60);
  }
  return data;
}
//----------------------------------Отправка бита в шину-----------------------------------------
void oneWireWriteBit(uint8_t data)
{
  if (data) {
    WIRE_LO;
    _delay_us(5);
    WIRE_HI;
    _delay_us(60);
  }
  else {
    WIRE_LO;
    _delay_us(60);
    WIRE_HI;
    _delay_us(5);
  }
}
//--------------------------------- Чтение бита с шины--------------------------------------------
uint8_t oneWireReadBit(void)
{
  uint8_t data = 0;
  WIRE_LO;
  _delay_us(2);
  WIRE_HI;
  _delay_us(8);
  data = WIRE_CHK;
  _delay_us(60);
  return data;
}
