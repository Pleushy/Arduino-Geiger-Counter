// ESP8266 WEMOS D1 code
// for RadiationD v1.1 (CAJOE)
// geiger counter
// and ST7735 TFT screen (128*160)
// by Ari :3

#include <ESP8266WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "images.h"

// #define USE_SERIAL

// screen
const uint16_t tft_height = 128;
const uint16_t tft_width = 160;
const uint8_t tft_cs = 4; // D2
const uint8_t tft_dc = 5; // D1
const uint8_t tft_rst = 16; // D0
const uint8_t tft_char_height = 14;
const uint8_t tft_char_width = 14;
uint16_t last_log_pixel[] = {0,0};
Adafruit_ST7735 tft(tft_cs, tft_dc, tft_rst);

// geiger counter 
const uint8_t geiger_pin = 12; // D6
const uint16_t log_size = tft_width-14;
const float uSv_conv = 0.0057f; // rough conversion to uSv/hr on sts-5
volatile uint64_t counts = 0;
uint64_t cpm = 0;
uint64_t cps = 0;
float uSv = 0;
uint64_t last_time = 0;
uint64_t cps_log[log_size];
volatile uint64_t last_click_time = 0;
const uint64_t dead_time_us = 1000;
const uint64_t cps_levels[] = {
	0,8,80,800,1600
};
const uint64_t cpm_levels[] = {
	0,500,5000,50000,100000
};
const uint64_t uSv_levels[] = {
	0,600,3000,30000,50000
};
const uint16_t danger_level_colors[] = {
	0x1d9f34,
	0x44a11c,
	0xa6a519,
	0xb2651b,
	0xac1616
};

const uint16_t get_danger_color(uint8_t mode, uint64_t value) {
	for (uint8_t i = 0; i < 5; i++) {
		switch (mode) {
			case 0: // cps
			if (value >= cps_levels[i]) return danger_level_colors[i];
			break;
			case 1: // cpm
			if (value >= cpm_levels[i]) return danger_level_colors[i];
			break;
			case 2: // uSv
			if (value >= uSv_levels[i]) return danger_level_colors[i];
			break;
			default:
			return 0x000000;
			break;
		}
	}
	return 0x000000;
}

void show_image(const uint16_t *image, uint16_t x, uint16_t y) {
	uint16_t h = pgm_read_byte_near(image++), w = pgm_read_byte_near(image++), row, col = 0;
	tft.startWrite();
	tft.setAddrWindow(x,y,w,h);
	for (row=0; row<h; row++) {
		for (col=0; col<w; col++) {
			tft.writePixel(x + col, y + row, pgm_read_word(image++));
		}
	}
	tft.endWrite();
}

void IRAM_ATTR tube_impulse() {
	unsigned long current_time = micros();
	if ((current_time - last_click_time) > dead_time_us) {
		counts++;
		last_click_time = current_time;
	}
}

void setup() {
	// wifi off for performance reasons
	WiFi.mode(WIFI_OFF);
	WiFi.forceSleepBegin();
	delay(1);

	// screen init
	tft.initR(INITR_BLACKTAB);
	tft.setRotation(1);
	tft.fillScreen(0x000000);
	tft.drawRoundRect(2,2,tft_width-4,tft_height-4,4,0x00a6aa);
	tft.setTextSize(2);
	tft.setCursor(8,8);
	tft.println("CPS: -");
	tft.setCursor(8,8+tft_char_height+4);
	tft.println("CPM: -");
	tft.setCursor(8,8+(tft_char_height+4)*2);
	tft.println("--.-- uSv/hr");
	tft.drawLine(3,8+(tft_char_height+4)*3,tft_width-4,8+(tft_char_height+4)*3,0x00a6aa);
	show_image(logo_radioactivity, tft_width-32-8, 8);

	// geiger counter init
	memset(cps_log, 0, sizeof(cps_log));
	pinMode(geiger_pin, INPUT);
	interrupts();
	attachInterrupt(digitalPinToInterrupt(geiger_pin), tube_impulse, FALLING);

	#ifdef USE_SERIAL
		Serial.begin(115200);
		Serial.println("\n\n\n========== BEGIN LOG ==========");
	#endif
}

void loop() {
	if ((millis() - last_time) >= 1000) {
		// cps
		noInterrupts();
		cps = counts;
		counts = 0;
		interrupts();

		// cps_log
		for (uint16_t i = 1; i < log_size; i++) {
			cps_log[i-1] = cps_log[i];
		}
		cps_log[log_size-1] = cps;

		// cpm
		cpm = 0;
		for (uint16_t i = log_size-60; i < log_size; i++) cpm += cps_log[i];

		// uSv
		uSv = uSv_conv * cpm;

		// set time
		last_time = millis();

		// send to tft screen
		// cps
		tft.setCursor(8+tft_char_width*4,8);
		tft.fillRect(8+tft_char_width*4,8,tft_char_width*4,tft_char_height,0x000000);
		tft.setTextColor(get_danger_color(0,cps));
		if (cps >= 10000) {
			tft.print(cps/1000);
			tft.println("K");
		} else {
			tft.println(cps);
		}

		// cpm
		tft.setCursor(8+tft_char_width*4,8+tft_char_height+4);
		tft.fillRect(8+tft_char_width*4,8+tft_char_height+4,tft_char_width*4,tft_char_height,0x000000);
		tft.setTextColor(get_danger_color(1,cpm));
		if (cpm >= 10000) {
			tft.print(cpm/1000);
			tft.println("K");
		} else {
			tft.println(cpm);
		}

		// uSv
		tft.setCursor(8,8+(tft_char_height+4)*2);
		tft.fillRect(8,8+(tft_char_height+4)*2,tft_char_width*5,tft_char_height,0x000000);
		tft.setTextColor(get_danger_color(2,(uint64_t){uSv*1000}));
		tft.println(uSv);

		// log graph
		tft.fillRect(6,16+(tft_char_height+4)*3,tft_width-12,(tft_char_height+4)*3, 0x000000);
		last_log_pixel[1] = 0;
		tft.startWrite();
		for (uint16_t i = 0; i < log_size; i++) {
			uint16_t current = (cps_log[i] * 10)/(cps == 0 ? 1 : cps);
			if (current >= (tft_char_height+4)*3) {
				current = (tft_char_height+4)*3;
			}
			uint16_t x = tft_width-8-i, y = tft_height-8-current;
			if (last_log_pixel[1] != 0) {
				tft.writeLine(last_log_pixel[0], last_log_pixel[1], x, y, 0xffffff);
			}
			last_log_pixel[0] = x; last_log_pixel[1] = y;
		}
		tft.endWrite();

		// serial debug
		#ifdef USE_SERIAL
			Serial.print("CPS: ");
			Serial.println(cps);
			Serial.print("CPM: ");
			Serial.println(cpm);
			Serial.print("uSv/hr: ");
			Serial.println(uSv);
			Serial.print("Log (20): ");
			for (int i = log_size-20; i < log_size; i++) {
				Serial.print(cps_log[i]);
				Serial.print(" ");
			}
			Serial.println("\n");
		#endif
	}
}
