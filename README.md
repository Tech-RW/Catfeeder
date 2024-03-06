This project is under construction. If you have any questions, please let me know.

I have 2 cats. Puck and Saar (as in Sarah). Lovely animals and each with a separate diet. One has a food intolerance and requires special food, the other tends to overeat. In practice, this meant they both received the same food and one would eat the other's food unless I stayed with them and fed them at fixed times, but that didn't work for me. This was the motivation to develop the catfeeder. The catfeeder consists of a box with a rotating disc inside. On the disc are two food bowls (standard IKEA) hidden under the lid. When a cat approaches (this is detected via the RFID chip in their neck), the correct food bowl is rotated forward. As long as the cat remains near the RFID reader, the bowl stays in place. If the cat moves away, the food bowl remains in place for about 10 seconds (in case she returns) and then rotates away again.

Version 0.1, with mobile phone holder:

![catfeeder 1 0](https://github.com/Tech-RW/Catfeeder/assets/120517590/ba5172c5-142e-4839-818f-bfcabc51899f)

Because one of the cats tends to eat a lot, she has a tendency to consume all the food in the morning. To help her eat more evenly, a time slot has been set during which she cannot eat. There is one time slot, and this time slot can be activated per cat (does not work stand-alone, this needs a WiFi connection for time synchronisation).

Because the stepper motor is not precise enough (due to the flywheel effect, the disc can rotate further than desired), the number of steps the stepper motor takes cannot be relied upon. Therefore, an infrared reader has been added, which can read the position of the disc. Each position has its own 'barcode' indicating the orientation of the disc. Short-Long-Short (KLK) represents the home position, Long-Short-Short (LKK) represents cat 1, Long-Short-Long (LKL) represents cat 2. It doesn't matter which direction the disc rotates; the barcodes can be read from both sides. Once the correct barcode is read, the motor (and the disc) stop.

The catfeeder can communicate via MQTT with, for example, Home Assistant or Node-RED, but can also operate stand-alone (except for the time-slot). Instructions can be given to the catfeeder via MQTT (catfeeder subscribes to catfeeder.mqtt.instruction), and the catfeeder provides information to the MQTT listener (catfeeder.mqtt.publish). The instructions that can be given are:

- Bowl Saar: Rotate Saar's food bowl forward
- Bowl Puck: Rotate Puck's food bowl forward
- Bowl Home: Rotate the disc to the home position
- eatingWindowStartHour:xx: Adjust the hour at which the time slot should start. Default: 12
- eatingWindowStartMinute:xx: Adjust the minutes at which the time slot should start. Default; 5
- eatingWindowEndHour:xx: Adjust the hour at which the time slot should end. Default: 17
- eatingWindowEndMinute:xx: Adjust the minutes at which the time slot should end. Default: 55
- Reset ESP: Soft reset the module

Furthermore, information can be requested:

- Bowl Status: Position of the disc

The catfeeder provides the following information via MQTT:

- Saar detected: Saar has been registered by the RFID reader
- Saar gone: Saar has been away from the RFID reader for longer than 10 seconds
- Puck detected/gone: Same as above, but for Puck
- Duration: <string>: Provides information about the rotation of the disc (see elsewhere)
- Error: <string>: Indicates an error message (currently only when the disc cannot find the correct position and keeps rotating indefinitely, after about 20 seconds the disc stops rotating and generates the error message)
- Saar allowed: Saar is at the RFID reader and is allowed to eat (outside the time slot)
- Saar declined: Saar is at the RFID reader and is not allowed to eat (inside the time slot)
- Puck allowed/declined: Same as above, but for Puck
- RFID Count: Number of times the RFID chip has been read during eating. The RFID reader reads the chip approximately once per second.


Hardware:

- ESP32 module (WiFi) - DollaTek R3 D1 R32 ESP32 ESP-32 CH340G (https://www.amazon.nl/dp/B07HBNQ99H)
- RFID reader (134.2KHz) - LICHIFIT 134.2K AGV RFID Module FDXB (https://www.amazon.nl/dp/B0B8NXKPZB)
- Stepper motor: 28BYJ-48 (modified to Bi-polar for more torque) (https://www.amazon.nl/dp/B08MWCRJ4W, 1 piece, controllers not needed)
- Motor driver for bipolar driving: Akozon MOSFET driver module, 5V (https://www.amazon.nl/dp/B07MXXL2KW, 1 piece)
- Infrared detector: Youmile IR Infrared reader (https://www.amazon.nl/dp/B07PY3CVSV, 1 piece)
- Flange coupling: Flange Coupler-2 pcs M3 (https://www.amazon.nl/dp/B08FYGCVNK, 1 piece)
- Table bearing: Gorgeri Table Bearing made of high-quality aluminum alloy (8 inch x h 8.5 mm) (https://www.amazon.nl/dp/B07Y45TPQ2)
- Wood (9mm MDF)
- Breadboard cables
- XFLYP Mobile phone-holder (https://www.amazon.nl/dp/B0CCRVXXGB)

Additionally, several parts were made with a 3D printer:

- Motor holder (https://www.thingiverse.com/thing:6519428)
- Holder for RFID reader
- Cover plate for motor/flange 
- Barcode-elements (https://www.thingiverse.com/thing:6519440)
- Bowl-holders: (https://www.thingiverse.com/thing:6519420)


Under the hood: (without cover plate for flange and bowl-holders)
![IMG_6434](https://github.com/Tech-RW/Catfeeder/assets/120517590/2fae9dd7-6751-4545-bcf3-c66fe913d16f)

Holders for bowls:

![IMG_6449](https://github.com/Tech-RW/Catfeeder/assets/120517590/8ef9cbb5-eaa7-46d6-80f5-2d794ef86833)

Table bearing, Infrared reader, Steppenmotor and motor holder:

![IMG_6429](https://github.com/Tech-RW/Catfeeder/assets/120517590/0f6799cc-b744-4cbe-a1c3-e1ed2e8210cf)

'Barcode'
![IMG_6428](https://github.com/Tech-RW/Catfeeder/assets/120517590/d452829f-2f26-4a26-98e2-469788d8833a)




