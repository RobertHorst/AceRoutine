#ifndef ACE_ROUTINE_COMMAND_DISPATCHER_H
#define ACE_ROUTINE_COMMAND_DISPATCHER_H

#include <Arduino.h> // Serial
#include <AceRoutine.h>
#include "SerialReader.h"

namespace ace_routine {
namespace cli {

/**
 * A coroutine that reads lines from the Serial port, tokenizes the line on
 * whitespace boundaries, and calls the appropriate command handler to handle
 * the command. Command have the form "command arg1 arg2 ...", where the 'arg*'
 * can be any string.
 *
 * The calling code is expected to provide a mapping of the command string
 * (e.g. "list") to its command handler (CommandHandler). The CommandHandler is
 * called with the number of arguments (argc) and the array of tokens (argv),
 * just like the arguments of the C-language main() function.
 */
class CommandDispatcher: public Coroutine {
  public:
    /** Maximum number of tokens for a command including flags. */
    static const uint8_t ARGV_SIZE = 10;

    /** Signature for a command handler. */
    typedef void (*CommandHandler)(int argc, const char** argv);

    /**
     * A record of the command name and its handler. The helpString is the
     * "usage" string, excluding the name of the command itself to save space.
     * The name of the command will be automatically added by the 'help
     * command' handler.
     */
    struct DispatchRecord {
      CommandHandler handler;
      const char* name;
      const char* helpString;
    };

    /**
     * Constructor.
     *
     * @param serialReader Instance of SerialReader.
     * @param numCommands Number of entries in dispatchTable
     * @param dispatchTable An array of DispatchRecords.
     */
    CommandDispatcher(
            SerialReader& serialReader,
            uint8_t numCommands,
            const DispatchRecord* dispatchTable):
        mSerialReader(serialReader),
        mNumCommands(numCommands),
        mDispatchTable(dispatchTable) {
    }

  protected:
    static const uint8_t STATUS_SUCCESS = 0;
    static const uint8_t STATUS_BUFFER_OVERFLOW = 1;
    static const uint8_t STATUS_FLUSH_TO_EOL = 2;
    static const char DELIMS[];

    void printLineError(const char* line, uint8_t statusCode) {
      if (statusCode == STATUS_BUFFER_OVERFLOW) {
        Serial.print(F("BufferOverflow: "));
        Serial.println(line);
      } else if (statusCode == STATUS_FLUSH_TO_EOL) {
        Serial.print(F("FlushToEOL: "));
        Serial.println(line);
      } else {
        Serial.println(F("UnknownError"));
      }
    }

    /** Handle the 'help' command. */
    void helpCommandHandler(int argc, const char** argv) {
      if (argc == 2) {
        const char* cmd = argv[1];
        if (strcmp(cmd, "help") == 0) {
          Serial.println(F("Usage: help [command]"));
          return;
        }

        for (uint8_t i = 0; i < mNumCommands; i++) {
          const DispatchRecord* record = &mDispatchTable[i];
          if (strcmp(record->name, cmd) == 0) {
            Serial.print("Usage: ");
            Serial.print(cmd);
            Serial.print(' ');
            Serial.println(record->helpString);
            return;
          }
        }
        Serial.print(F("Unknown command: "));
        Serial.println(cmd);
      } else {
        Serial.println(F("Usage: help [command]"));
        Serial.print(F("Commands: help "));
        for (uint8_t i = 0; i < mNumCommands; i++) {
          const DispatchRecord* record = &mDispatchTable[i];
          Serial.print(record->name);
          Serial.print(' ');
        }
        Serial.println();
      }
    }

    /** Tokenize the given line and run the command handler. */
    void runCommand(char* line) {
      // Tokenize the line.
      const char* argv[ARGV_SIZE];
      int argc = tokenize(line, ARGV_SIZE, argv);
      if (argc == 0) return;
      const char* cmd = argv[0];

      // The 'help' command is built-in.
      if (strcmp(cmd, "help") == 0) {
        helpCommandHandler(argc, argv);
        return;
      }

      // Look for an entry in the dispatchTable.
      // NOTE: this is currently a linear scan O(N) which is good enough for
      // small number of commands. If we sorted the dispatchTable, we could do
      // a binary search for O(log(N)) and handle large number of commands.
      for (uint8_t i = 0; i < mNumCommands; i++) {
        const DispatchRecord* record = &mDispatchTable[i];
        if (strcmp(record->name, cmd) == 0) {
          record->handler(argc, argv);
          return;
        }
      }

      Serial.print(F("Unknown command: "));
      Serial.println(cmd);
    }

    /** Tokenize the line, returning the number of tokens. */
    int tokenize(char* line, int argvSize, const char** argv) {
      char* token = strtok(line, DELIMS);
      int argc = 0;
      while (token != nullptr && argc < argvSize) {
        argv[argc] = token;
        argc++;
        token = strtok(nullptr, DELIMS);
      }
      return argc;
    }

    virtual int runRoutine() override {
      bool isError;
      char* line;
      COROUTINE_LOOP() {
        COROUTINE_AWAIT(mSerialReader.getLine(&isError, &line));

        if (isError) {
          printLineError(line, STATUS_BUFFER_OVERFLOW);
          while (isError) {
            COROUTINE_AWAIT(mSerialReader.getLine(&isError, &line));
            printLineError(line, STATUS_FLUSH_TO_EOL);
          }
          continue;
        }

        runCommand(line);
      }
    }

    SerialReader& mSerialReader;
    const uint8_t mNumCommands;
    const DispatchRecord* const mDispatchTable;
};

}
}

#endif
