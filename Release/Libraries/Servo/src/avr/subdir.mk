################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/home/gaston/arduino-1.6.5-r5/libraries/Servo/src/avr/Servo.cpp 

LINK_OBJ += \
./Libraries/Servo/src/avr/Servo.cpp.o 

CPP_DEPS += \
./Libraries/Servo/src/avr/Servo.cpp.d 


# Each subdirectory must supply rules for building sources it contributes
Libraries/Servo/src/avr/Servo.cpp.o: /home/gaston/arduino-1.6.5-r5/libraries/Servo/src/avr/Servo.cpp
	@echo 'Building file: $<'
	@echo 'Starting C++ compile'
	"/home/gaston/arduino-1.6.5-r5/hardware/tools/avr/bin/avr-g++" -c -g -Os -fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics -MMD -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10605 -DARDUINO_AVR_NANO -DARDUINO_ARCH_AVR     -I"/home/gaston/arduino-1.6.5-r5/hardware/arduino/avr/cores/arduino" -I"/home/gaston/arduino-1.6.5-r5/hardware/arduino/avr/variants/eightanaloginputs" -I"/home/gaston/arduino-1.6.5-r5/hardware/arduino/avr/libraries/EEPROM" -I"/home/gaston/arduino-1.6.5-r5/libraries/LiquidCrystal" -I"/home/gaston/arduino-1.6.5-r5/libraries/LiquidCrystal/src" -I"/home/gaston/arduino-1.6.5-r5/libraries/Servo" -I"/home/gaston/arduino-1.6.5-r5/libraries/Servo/src" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -D__IN_ECLIPSE__=1 -x c++ "$<"  -o  "$@"   -Wall
	@echo 'Finished building: $<'
	@echo ' '


