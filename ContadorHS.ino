#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Servo.h>
#include <Arduino.h>
// ATMEL ATMEGA8 & 168 / ARDUINO
//
//
//                      TX1|    |VIN
//                      RX1|    |GND
//                      RST|    |RST
//                      GND|    |5V
// TACOMETRO        int0/D2|    |A7  Solo analogico CONTACTO
// STOP       PWM+/int1/D3*|    |A6  Solo analogico LIBRE
// LCD-RS                D4|    |A5/D19/SCL
// sACEL           PWM+/D5*|    |A4/D18/SDA 		T_DOW
// sSTOP           PWM+/D6*|    |A3/D17 			LCD-D7
// LCD-D5                D7|    |A2/D16 			LCD-D4
// LCD-D6                D8|    |A1/D15 			LCD-ENABLE
// T_OK            PWM+/D9*|    |A0/D14 			LIBRE
// LEDsERVICIO PWM+/SS/D10*|    |AREF
// RELAY     PWM+/MOSI/D11*|    |3.3v
// T_UP	           MISO/D12| \/ |D13/SCK/ledL
//                          USB
//LiquidCrystal	 (rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(4, 15, 16, 7, 8, 17);

unsigned long tiempoAnt1;
volatile unsigned long tiempoAntInt;
volatile unsigned long RMicros;
int RPM;
bool marcha = false;
byte pasada = 2;

/********************************************************************/
const int regimen = 1200; //regimen de rpm que tendra que mantener
int aceleradorArranque = 3; //POSICION PREESTABLECIDA DEL ACELERADOR PARA ARRANCAR
const int SPstop = 180;
const int SPrun = 10;
const int SPacelMin = 10;
const int SPacelMax = 180;
int acelerador;
/*******************************************************************/

char accion = ' ';
Servo sStop;
Servo sAcel;

//TIEMPO EN SEGUNDO
unsigned long usoActual; //tiempo de marcha actual
//unsigned long usoAnterior;
unsigned long usoTotal; // tiempo de marcha total =
long restanteServicio; // tiempo que falta para realizar un servicio de mantenimiento
long alarmaPreServicio; // Timpo restante donde el led empiza a parpadear (10 % )

//const int EEusoAnterior = 3; POSICIONES DE MEMORIA EEPROM
const int EEusoTotal = 7;
const int EErestanteServicio = 11;
const int EEalarmaPreServicio = 15;

const int sensorGiro = 2; // interrupcion 0
const int paradaEmergencia = 3; // interrupcion 1
const int ledL = 13;
const int relay = 11;
const int ledServicio = 10;

//teclado
const int t_dow = 18; //negro A4
const int t_ok = 9; //amarillo D9
const int t_up = 12; //rojo D12

//1413
void setup() {
	Serial.begin(9600);
	Serial.println("------------------------ RUN ------------------------");
	lcd.begin(16, 2);
	lcd.print("espere..");
	tiempoAnt1 = millis();

	pinMode(relay, OUTPUT);
	pinMode(ledServicio, OUTPUT);
	pinMode(paradaEmergencia, INPUT);
	digitalWrite(ledServicio, HIGH);
	digitalWrite(relay, LOW);

	pinMode(t_up, INPUT_PULLUP);
	pinMode(t_ok, INPUT_PULLUP);
	pinMode(t_dow, INPUT_PULLUP);

	sStop.attach(6);
	sAcel.attach(5);

	sStop.write(SPstop);
	sAcel.write(SPacelMin);

	//recupera contadores de la eeprom
	EEPROM.get(EEusoTotal, usoTotal);
	delay(10);
	EEPROM.get(EErestanteServicio, restanteServicio);
	delay(10);
	EEPROM.get(EEalarmaPreServicio, alarmaPreServicio);
	delay(10);

	Serial.println("Uso Total: " + String(usoTotal));
	Serial.println("Restan servicio: " + String(restanteServicio));
	Serial.println("Alarma pre servicio: " + String(alarmaPreServicio));

	usoActual = 300 - 30;
	reset();
	resetTotal();
	testServo();
	avisoPreArranque();
	chequerPoteccion();
	attachInterrupt(digitalPinToInterrupt(sensorGiro), tacometro, FALLING);
	digitalWrite(relay, HIGH);
	delay(1000);
	digitalWrite(ledServicio, LOW);
	sStop.write(SPrun);
	tiempoAntInt = 0;
	RMicros = 0;
	RPM = 0;
}

void chequerPoteccion() {
	if (!digitalRead(paradaEmergencia)) {
		lcd.clear();
		lcd.print("Proteccion: OFF");
		lcd.setCursor(0, 1);
		lcd.print("Continuar   [OK]");
		while (digitalRead(t_ok)) {
			delay(200);
		}
		while (!digitalRead(t_ok)) {
			delay(50);
		}
	}
}

void loop() {
	long ini = millis();
	// Espera Ignicion
	while (RMicros == 0 && marcha == false) {
		if (usoActual <= 10) {
			usoActual = 0;
		}
		lcd.setCursor(0, 0);
		lcd.print("ARRANQUE!!!    #");
		delay(200);
		if (!digitalRead(t_ok)) {
			if (aceleradorArranque < 15) {
				aceleradorArranque++;
			}
		} else {
			if (!digitalRead(t_dow)) {
				if (aceleradorArranque > 1) {
					aceleradorArranque--;
				}
			} else {
				if (!digitalRead(t_up)) {
					imprimir();
					delay(7000);
				}
			}
		}
		//map(value, fromLow, fromHigh, toLow, toHigh)
		acelerador = map(aceleradorArranque, 1, 15, SPacelMin, SPacelMax);
		sAcel.write(acelerador);
		lcd.setCursor(0, 1);
		lcd.print("                ");
		lcd.setCursor(0, 1);
		for (int i = 0; i < aceleradorArranque; i++) {
			lcd.print("*");
		}
		lcd.setCursor(0, 0);
		lcd.print("ARRANQUE!!!     ");
		delay(200);

		//aca es donde espera un minuto cuando el llavero es: "apagado" o "contacto + arranque"
		int r = 0;
		while (voltaje() < 6 && r <= 120) {
			delay(500);
			r++;
			Serial.println(
					String(acelerador) + ";" + formatoTiempo(usoActual) + ";"
							+ RPM + ";" + voltaje() + ";" + accion);
		}
		// cuando sale del while de arriba puede ser por "tiempo de espera agotado" o por "llavero en contacto"
		if (voltaje() < 6 && r >= 120) {
			// si el contacto es "apagado" o "contacto mas arranque" y el tiempo de espera es mayor o igual a 1 minuto apaga
			apagar(false);
		}
		Serial.println(
				String(acelerador) + ";" + formatoTiempo(usoActual) + ";" + RPM
						+ ";" + voltaje() + ";" + accion);
	}
	//Ignicion sastifactoria :D
	marcha = true;

	if ((tiempoAnt1 + 250) <= millis()) {
		tiempoAnt1 = millis();

		detachInterrupt(digitalPinToInterrupt(sensorGiro));
		if (RMicros > 0) {
			RPM = (1000000.00 / (float) RMicros) * 60.00;
			RMicros = 0;
		}
		//Serial.println(RMillis + " - " + tiempoAnt1);
		attachInterrupt(digitalPinToInterrupt(sensorGiro), tacometro, FALLING);

		// Si rpm es mayor a cero cuenta un segundo de uso
		pasada++;
		if (RPM > 300 && pasada >= 4) {
			usoActual++;
			usoTotal++;
			restanteServicio--;
			pasada = 0;
		}

		if (restanteServicio > 1) {
			if (restanteServicio <= alarmaPreServicio) {
				digitalWrite(ledServicio, !digitalRead(ledServicio));
			}
		} else {
			digitalWrite(ledServicio, HIGH);
		}

		// espera 5 minutos para entrar en regimen
		// 5 minutos = 300 segundos
		accion = '=';
		if (usoActual > 300 && RPM > 300) {
			if (((regimen - RPM) > 10) && acelerador < SPacelMax) {
				//acelerar
				accion = '+';
				if ((regimen - RPM) > 100) {
					acelerador = acelerador + 10;
				} else {
					if ((regimen - RPM) > 30) {
						acelerador = acelerador + 5;
					} else {
						if ((regimen - RPM) > 20) {
							acelerador = acelerador + 3;
						} else {
							if ((regimen - RPM) > 10) {
								acelerador = acelerador + 1;
							}
						}
					}
				}
			} else {
				if (((RPM - regimen) > 10) && acelerador > SPacelMin) {
					//desacelerar
					accion = '-';
					if ((RPM - regimen) > 100) {
						acelerador = acelerador - 10;
					} else {
						if ((RPM - regimen) > 30) {
							acelerador = acelerador - 5;
						} else {
							if ((RPM - regimen) > 20) {
								acelerador = acelerador - 3;
							} else {
								if ((RPM - regimen) > 10) {
									acelerador = acelerador - 1;
								}
							}
						}
					}
				}
			}
			if (acelerador >= SPacelMax) {
				accion = 'M';
			}
			if (acelerador <= SPacelMin) {
				accion = 'm';
			}
		} else {
			accion = 'E';
			if (usoActual > 2 && RPM > 300) {
				acelerador = 0;
			}
		}
		sAcel.write(acelerador);

		if (RPM == 0) {
			marcha = false;
		}
		imprimir();
		RPM = 0;

		// no hay voltaje por contacto?? apagar
		if (voltaje() < 6) {
			apagar(false);
		}
		// parada de emergencia
		if (digitalRead(paradaEmergencia) && usoActual>2) {
			apagar(true);
		}

		unsigned int ini = millis() - ini;

		Serial.println(
				String(acelerador) + ";" + formatoTiempo(usoActual) + ";" + RPM
						+ ";" + voltaje() + ";" + accion + ";" + String(ini));
	}

}

float voltaje() {
	const float v1 = 5; // valor real de la alimentacion de Arduino, Vcc
	const float r1 = 1200; //1k2 original: 1M
	const float r2 = 130; //100 Original: 100K
	const int voltimetro = 7; //pin cero analogico
	float v = (analogRead(voltimetro) * v1) / 1024.0;
	v = v / (r2 / (r1 + r2));
	//Serial.println("Max volt:" + String((int) (v1 / (r2 / (r1 + r2)))));
	//Serial.println("Voltaje " + String(v));
	return v;
}

void tacometro() {
	//contRPM++;
	RMicros = micros() - tiempoAntInt;
	tiempoAntInt = micros();
	digitalWrite(ledL, !digitalRead(ledL));
}

//Guardar el estado de los contadores si hay modificaciones. Cortar la corriente del relay
void apagar(bool stop) {
	//desactivar todas las interrupciones
	detachInterrupt(digitalPinToInterrupt(sensorGiro));
	lcd.clear();
	if (stop) {
		lcd.print("***PROTECCION***");
		digitalWrite(ledServicio, HIGH);
	} else {
		lcd.print("contacto OFF");
	}

	sAcel.write(SPacelMin);
	delay(200);
	sStop.write(SPstop);

	//Resguardar en la EEPROM los relojes si hay modificaciones MAYORES A 2 SEGUNDOS
	Serial.println("uso actual: " + String(usoActual));
	if (usoActual > 10) {
		Serial.println("guardando: " + String(restanteServicio));

		EEPROM.put(EErestanteServicio, restanteServicio);
		delay(10);
		EEPROM.put(EEusoTotal, usoTotal);
		delay(10);

		//EEPROMWritelong(EErestanteServicio, restanteServicio);
		//EEPROMWritelong(EEusoAnterior, usoActual);
		//EEPROMWritelong(EEusoTotal, usoTotal);
	}
	//Esperar diez segundo y desactivar el relay
	delay(10000);
	digitalWrite(relay, LOW);
	delay(1000);
	// en este punto no deberia haber mas energuia
	if (!stop) {
		lcd.setCursor(0, 1);
		lcd.print("**ERROR** relay");
	}
	while (true) {

	}
}

String formatoTiempoNegativo(long tiempo) {
	String stringTiempo = "";
	if (tiempo < 0) {
		tiempo = tiempo * -1;
		stringTiempo = "-";
	}
	//Serial.println("long tiempo: " + String((unsigned long) tiempo));
	stringTiempo = stringTiempo + formatoTiempo((unsigned long) tiempo);
	return stringTiempo;
}

String formatoTiempo(unsigned long tiempo) {
	String stringTiempo = "";

	byte segundos;
	byte minutos;
	unsigned int horas;

	// calcula los campos hh mm ss
	segundos = tiempo % 60;
	unsigned long resguardo = tiempo / 60;
	minutos = resguardo % 60;
	horas = tiempo / 3600;

	if (horas < 10) {
		//imprimir "0+horas"
		stringTiempo = "0" + String(horas);
	} else {
		//imprimir "horas"
		stringTiempo = String(horas);
	}

	//imprimir ":"
	stringTiempo = stringTiempo + ":";
	if (minutos < 10) {
		//imprimir "0+minutos"
		stringTiempo = stringTiempo + "0" + String(minutos);
	} else {
		//imprimir "minutos"
		stringTiempo = stringTiempo + String(minutos);
	}
	//imprimir ":"
	stringTiempo = stringTiempo + ":";
	if (segundos < 10) {
		//imprimir "0+segundos"
		stringTiempo = stringTiempo + "0" + String(segundos);
	} else {
		//imprimir segundos
		stringTiempo = stringTiempo + String(segundos);
	}
	return stringTiempo;
}

void imprimir() {
	lcd.clear();
	if (!digitalRead(t_up)) { //mostrar tiempo total
		lcd.print("TOTAL MARCHA:");
		lcd.setCursor(0, 1);
		lcd.print(" " + formatoTiempo(usoTotal));
	} else {
		if (!digitalRead(t_dow)) { //mostrar tiempo para servicio
			if (restanteServicio > 0) {
				lcd.print("****SERVICIO****");
				lcd.setCursor(0, 1);
				lcd.print("FALTA " + formatoTiempoNegativo(restanteServicio)); //FALTA -00:00:00
			} else {
				lcd.print("*HACER SERVICIO*");
				lcd.setCursor(0, 1);
				lcd.print("EXCEDE " + formatoTiempoNegativo(restanteServicio)); //EXCEDE -00:00:00
			}
		} else { // mostrar tiempo actual voltaje y rpm
			lcd.print("HOY: " + formatoTiempo(usoActual) + " " + accion); //HOY: 00:00:00
			lcd.setCursor(0, 1);
			lcd.print(String(voltaje()) + "V");
			lcd.print(" ");
			lcd.print(String(RPM) + " RPM");
		}
	}

}

void avisoPreArranque() {
	if (restanteServicio <= alarmaPreServicio) {
		lcd.clear();
		if (restanteServicio > 0) {
			lcd.print("****SERVICIO****");
			lcd.setCursor(0, 1);
			lcd.print("FALTA " + formatoTiempoNegativo(restanteServicio)); //FALTA -00:00:00
		} else {
			lcd.print("*HACER SERVICIO*");
			lcd.setCursor(0, 1);
			lcd.print("EXCEDE " + formatoTiempoNegativo(restanteServicio)); //EXCEDE -00:00:00
		}
		digitalWrite(ledServicio, HIGH);
		delay(6000);
	}
}

void reset() { //Se ingresa preionando reset/ok solo cuando reloj llego a un valor negativo
	if (restanteServicio < 0) {
		if (digitalRead(t_up) && digitalRead(t_dow) && !digitalRead(t_ok)) {
			sStop.write(SPstop);
			lcd.print("*RESET*");
			delay(2000);
			while (!digitalRead(t_ok)) {
				delay(200);
			}
			// minimo valor de un entero sin signo 65,535
			unsigned int cont = 0;
			bool fin = false;
			while (!fin) {
				lcd.clear();
				lcd.print("ingresar Horas");
				lcd.setCursor(0, 1);
				if (!digitalRead(t_up)) { // sumar +10 hs
					cont = cont + 10;
				}
				if (!digitalRead(t_dow)) { // restar 10 hs
					cont = cont - 10;
				}
				lcd.print(String(cont) + " HS");
				if (!digitalRead(t_ok)) {
					lcd.print(" ok");
					// maximo valor de un long 2,147,483,647
					// 596523 horas aproximadamente es el maximo que se le puede asignar al reloj
					restanteServicio = (long) cont * 3600;
					alarmaPreServicio = (long) restanteServicio * 0.10;
					Serial.println(
							"restan servicio: " + String(restanteServicio));
					Serial.println(
							"alarma preServicio: " + String(alarmaPreServicio));
					EEPROM.put(EErestanteServicio, restanteServicio);
					delay(10);
					EEPROM.put(EEalarmaPreServicio, alarmaPreServicio);
					delay(10);
					fin = true;
				}
				delay(200);
			}
			sStop.write(SPrun);
		}
	}
}

void testServo() { //se ingresa presionando up y dow
	if (!digitalRead(t_up) && !digitalRead(t_dow) && digitalRead(t_ok)) {
		acelerador = SPacelMin;
		int stop = SPrun;
		byte selector = 0;
		lcd.clear();
		lcd.print("probar servos");
		delay(2000);
		while (true) {
			lcd.clear();
			if (selector == 0) {
				lcd.print("Acel[+/-]: ");
				if (!digitalRead(t_up) && acelerador < SPacelMax) {
					acelerador++;
				}
				if (!digitalRead(t_dow) && acelerador > SPacelMin) {
					acelerador--;
				}
				sAcel.write(acelerador);
				lcd.print(String(acelerador));
			} else {
				if (selector == 1) {
					lcd.print("Stop[+/-]: ");
					if (!digitalRead(t_up) && stop < SPstop) {
						stop++;
					}
					if (!digitalRead(t_dow) && stop > SPrun) {
						stop--;
					}
					sStop.write(stop);
					lcd.print(String(stop));
				}
			}
			lcd.setCursor(0,1);
			lcd.print("Cambiar     [OK]");
			if (!digitalRead(t_ok)) {
				if(selector<1){
					selector++;
				}else{
					selector=0;
				}
			}
			delay(200);
		}
	}
}

void resetTotal() { //se ingresa presionando ok y up
	if (!digitalRead(t_up) && digitalRead(t_dow) && !digitalRead(t_ok)) {
		int cont = 0;
		bool fin = false;
		while (!fin) {
			lcd.clear();
			lcd.print("RESET TOTAL  50");
			lcd.setCursor(0, 1);
			if (!digitalRead(t_dow)) {
				cont++;
			}
			lcd.print("CUIDADO!!! " + String(cont));
			if (!digitalRead(t_ok) && (cont == 50)) {
				restanteServicio = (long) 0;
				alarmaPreServicio = (long) 0;
				usoTotal = (long) 0;
				EEPROM.put(EErestanteServicio, restanteServicio);
				delay(10);
				EEPROM.put(EEalarmaPreServicio, alarmaPreServicio);
				delay(10);
				EEPROM.put(EEusoTotal, usoTotal);
				delay(10);
				fin = true;
			}
			delay(200);
		}
	}
}

/*
 //This function will write a 4 byte (32bit) long to the eeprom at
 //the specified address to address + 3.
 void EEPROMWritelong(int address, long value) {
 //Decomposition from a long to 4 bytes by using bitshift.
 //One = Most significant -> Four = Least significant byte
 byte four = (value & 0xFF);
 byte three = ((value >> 8) & 0xFF);
 byte two = ((value >> 16) & 0xFF);
 byte one = ((value >> 24) & 0xFF);

 //Write the 4 bytes into the eeprom memory.
 EEPROM.write(address, four);
 EEPROM.write(address + 1, three);
 EEPROM.write(address + 2, two);
 EEPROM.write(address + 3, one);
 }

 long EEPROMReadlong(int address) {
 //Read the 4 bytes from the eeprom memory.
 long four = EEPROM.read(address);
 long three = EEPROM.read(address + 1);
 long two = EEPROM.read(address + 2);
 long one = EEPROM.read(address + 3);

 //Return the recomposed long by using bitshift.
 return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF)
 + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
 }
 */
