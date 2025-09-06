/*
 * Plant.h
 *
 *  Created on: Aug 12, 2025
 *      Author: Omar Elsaghir
 */

#ifndef PLANT_H_
#define PLANT_H_

// Libraries ------------------------------------------------------------------

#include <stdint.h>
#include "plant.h"

// Defines --------------------------------------------------------------------

// Variables ------------------------------------------------------------------

// Structures -----------------------------------------------------------------

// Functions ------------------------------------------------------------------

// Water pump
void setWaterPumpSpeed(uint16_t duty);

uint32_t getHX711Raw(void);

// Plant
void initPlant(void);
void getPlantData(uint16_t *lux, uint8_t *temp, uint8_t *hum, uint16_t *moist, uint16_t *volume);


#endif /* PLANT_H_ */
