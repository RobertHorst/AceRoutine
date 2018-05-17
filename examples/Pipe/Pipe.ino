/*
 * A sketch that illustrates using two AceRoutines and a circular buffer to
 * create a pipe between the two routines. Not sure if this is of any practical
 * use though.
 */

#include <Arduino.h>
#include <AceRoutine.h>
using namespace ace_routine;

/** A circular buffer of BUF_SIZE. */
class Channel {
  public:
    static const uint8_t BUF_SIZE = 64;

    void write(char c) {
      if (mNumElements >= BUF_SIZE) return;
      mBuffer[mEnd] = c;
      if (++mEnd >= BUF_SIZE) mEnd = 0;
      mNumElements++;
    }

    char read() {
      if (mNumElements == 0) return 0;
      char element = mBuffer[mStart];
      if (++mStart >= BUF_SIZE) mStart = 0;
      mNumElements--;
      return element;
    }

    bool isWritePossible() { return mNumElements < BUF_SIZE; }

    bool isReadPossible() { return mNumElements > 0; }

    char mBuffer[BUF_SIZE];
    uint8_t mStart = 0;
    uint8_t mEnd = 0;
    uint8_t mNumElements = 0;
};

Channel channel;

ROUTINE(reader) {
  ROUTINE_LOOP() {
    ROUTINE_AWAIT(Serial.available() > 0 && channel.isWritePossible());
    // copy as many characters as possible without yielding
    while (Serial.available() > 0 && channel.isWritePossible()) {
      char c = Serial.read();
      channel.write(c);
    }
  }
}

ROUTINE(writer) {
  ROUTINE_LOOP() {
    ROUTINE_AWAIT(channel.isReadPossible());
    char c = channel.read();
    Serial.print(c);
  }
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  while (!Serial); // Leonardo/Micro

  RoutineScheduler::setup();
}

void loop() {
  RoutineScheduler::loop();
}
