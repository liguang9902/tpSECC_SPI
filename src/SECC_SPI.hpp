#ifndef _SPI_H_INCLUDED
#define _SPI_H_INCLUDED

#include <stdlib.h>
#include "pins_arduino.h"
#include "esp32-hal-spi.h"
#include "Stream.h"
#include "driver\spi_master.h"
#include "string.h"
#define SPI_HAS_TRANSACTION

class SPISettings
{
public:
    SPISettings() :_clock(1000000), _bitOrder(SPI_MSBFIRST), _dataMode(SPI_MODE0) {}
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) :_clock(clock), _bitOrder(bitOrder), _dataMode(dataMode) {}
    uint32_t _clock;
    uint8_t  _bitOrder;
    uint8_t  _dataMode;
};

class SPIClass
{
private:
    int8_t _spi_num;
    spi_t * _spi;
    bool _use_hw_ss;
    int8_t _sck;
    int8_t _miso;
    int8_t _mosi;
    int8_t _ss;
    uint32_t _div;
    uint32_t _freq;
    bool _inTransaction;
    void writePattern_(const uint8_t * data, uint8_t size, uint8_t repeat);

public:
    SPIClass(uint8_t spi_bus=HSPI);
    void begin(int8_t sck=-1, int8_t miso=-1, int8_t mosi=-1, int8_t ss=-1);
    void end();

    void setHwCs(bool use);
    void setBitOrder(uint8_t bitOrder);
    void setDataMode(uint8_t dataMode);
    void setFrequency(uint32_t freq);
    void setClockDivider(uint32_t clockDiv);
    
    uint32_t getClockDivider();

    void beginTransaction(SPISettings settings);
    void endTransaction(void);
    void transfer(uint8_t * data, uint32_t size);
    uint8_t transfer(uint8_t data);
    uint16_t transfer16(uint16_t data);
    uint32_t transfer32(uint32_t data);
  
    void transferBytes(const uint8_t * data, uint8_t * out, uint32_t size);
    void transferBits(uint32_t data, uint32_t * out, uint8_t bits);

    void write(uint8_t data);
    void write16(uint16_t data);
    void write32(uint32_t data);
    void writeBytes(const uint8_t * data, uint32_t size);
    void writePixels(const void * data, uint32_t size);//ili9341 compatible
    void writePattern(const uint8_t * data, uint8_t size, uint32_t repeat);

   
    int available(void);
    //int availableForWrite(void);
    int peek(void);
    int read(void);
    void flush(void);
    //void flush( bool txOnly);
    //size_t write(uint8_t);
    //size_t write(const uint8_t *buffer, size_t size);
    void  SPIString(const uint8_t *data,size_t length);

    esp_err_t spi_write(spi_device_handle_t spi, uint8_t *data, uint8_t  len);

    spi_t * bus(){ return _spi; }
};

extern SPIClass SPI;

#endif
