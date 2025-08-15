# Morse_code_communication_glove

This project is a secure, LoRa-based communication system for soldiers using a glove interface to send and receive Morse code messages. It enables silent, encrypted, and real-time communication in the field, with haptic feedback for message reception.

Features: 
1. Morse Code Input via FSR sensors on the glove.
2. AES Encryption for secure message transmission.
3. LoRa SX1278 Module for long-range wireless communication.
4. Sender Verification to ensure authenticity.
5. Haptic Feedback using an ERM vibration motor for received messages. 

Hardware Requirements:
1.ESP32 microcontroller
2. LoRa SX1278 module
3. FSR (Force-Sensitive Resistor) sensor(s)
4. ERM vibration motor
5. Li-Po battery & charging module
6. Optional: Bone conduction speaker (future scope)

Workflow: 
A. Transmitter side (TX)

1.Detect input via FSR and confirm with short vibration feedback.
2.Classify and store inputs as letters or words in Morse code format.
3.Provide 2 vibration feedbacks when a word is completed.
4.Store Morse code in an input buffer.
5.Initiate transmission with 3 taps from the user.
6.Encrypt the buffered Morse code using AES encryption.
7.Send encrypted message via LoRa transceiver.

B. Receiver side (RX)

1.Receive encrypted packet via LoRa transceiver.
2.Store packet in buffer.
3.Check acknowledgement status:
4.If acknowledged → give long vibration feedback.
5.If not acknowledged → give continuous vibration feedback.
6.Decrypt received message.
7.Verify authenticity:
If tampered or from unknown source → clear buffer and send 3 vibration signals to transmitter to resend.
If authentic → output Morse message.

Impact in Military Systems:
1.Enables silent, secure, and low-power communication without relying on cellular or internet infrastructure.
2.Reduces detection risk by using non-audible haptic feedback.
3.Enhances team coordination during high-noise combat or covert operations.

Future Scope:

1.Headquarter & Team Broadcast – Send encrypted messages to multiple recipients simultaneously.
2.Extended LoRa Range – Hardware tuning and antenna upgrades for longer communication distances.
3.Bone Conduction Speaker – Deliver Morse feedback directly via bone conduction, ensuring soldiers don’t miss messages during intense combat.
