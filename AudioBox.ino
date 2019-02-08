/***************************************************
  This is an example for the Adafruit VS1053 Codec Breakout

  Designed specifically to work with the Adafruit VS1053 Codec Breakout
  ----> https://www.adafruit.com/products/1381

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer =  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

#define VOLUME_PIN A1 // The volume pin which is connected to the potentiometer.
#define PIN_REPLACE_FOR_11 A0 // The volume pin which is connected to the potentiometer.
#define MAX_VOLUME 5 // Volume is the lower the louder.
#define MIN_VOLUME 100 // Volume is the lower the louder.

#define BYTE_MAX 255 // To check against an unset byte type.

#define PREV_BUTTON_ID 10 // The number of the button for playing the previous.
#define NEXT_BUTTON_ID 11 // The number of the button for playing the next.

#define READY_FOR_INPUT  1 // No button is pressed and we can receive a button input.
#define HANDLING_INPUT  2 // For making sure we only handle one button input.

#define SESSION_TEXT_FILE_PATH "store.txt"

int currentVolume = MIN_VOLUME;
byte pinArrayLength = 4;
int pinArray[] = {2, 5, 8, 9};
int currentFileIndex = -1;
byte currentDirIndex = BYTE_MAX;
byte inputstate = READY_FOR_INPUT;
bool startingUp = true;
bool resetAtStart = false;
void setup() {

  Serial.begin(9600);

  if (! musicPlayer.begin()) { // initialise the music player
    Serial.println("Couldn't find VS1053, do you have the right pins defined?");
    while (1);
  }

  Serial.println("VS1053 found");

  if (!SD.begin(CARDCS)) {
    Serial.println("SD failed, or not present");
    while (1);  // don't do anything more
  }

  SetupPins();
  BuildPlaylistIndex();
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);


  resetAtStart = !digitalRead(PIN_REPLACE_FOR_11) && !digitalRead(9);
  Serial.print("Boot value ");
  Serial.println(resetAtStart );
  if (!resetAtStart) {
    musicPlayer.playFullFile("intro.mp3");
  }else{
    musicPlayer.sineTest(0x44, 600);
  }
}


void SetupPins() {

  pinMode(PIN_REPLACE_FOR_11, INPUT_PULLUP);

  for (uint8_t count = 0; count < pinArrayLength; count++) {
    pinMode(pinArray[count], INPUT_PULLUP);
  }

  for (uint8_t i = 0; i < 7; i++) {
    musicPlayer.GPIO_digitalWrite(i, HIGH);
  }
}

// I could consider storing this data in a file on the SD, so we dont need to build the index at startup.
// It would be faster to load the file and parse it. It is also feasible because we create the data on the SD ourselves.
int sumFilesPerFolderCache[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
void BuildPlaylistIndex() {

  for (int i = 0; i < 10; i++) {
    String path =  "audio/" + String(i);

    File audioDirectory = SD.open(path, O_READ);
    audioDirectory.rewindDirectory();
    int sumFiles = 0;
    while ( true )
    {
      File entry =  audioDirectory.openNextFile();
      if ( entry )
      {
        String file_name = entry.name();
        //        Serial.println(file_name);
        entry.close();

        const char tilde = '~';

        if ( file_name.indexOf(tilde) == -1 )
        {
          ++sumFiles;
        }
      } else {
        // not sure if this call is need, we should remove it and test it.
        audioDirectory.rewindDirectory();
        audioDirectory.close();
        break;
      }
    }
    sumFilesPerFolderCache[i] = sumFiles;
  }
}


void loop() {
  if (startingUp) {
    if (musicPlayer.readyForData()) {
      startingUp = false;
      ContinuePlayingFromSession();
    }
    return;
  }

  UpdateVolume();

  if (startingUp && resetAtStart) {
    resetAtStart = false;
    startingUp = false;
    Reset(0, 0);
    PlayNext();
    return;
  }
  
  if (musicPlayer.readyForData()) {
    // the current track has stopped playing so we continue
    if (UpdateFileCursor(currentDirIndex, 1)) {
      PlayNext();
    } else {
      Reset(currentDirIndex, 0);
      PlayNext();
    }
  }

  byte buttonPressed = GetCurrentPressedButton();
  if (inputstate == HANDLING_INPUT && buttonPressed == BYTE_MAX) {
    inputstate = READY_FOR_INPUT;
  } else if (inputstate == READY_FOR_INPUT && buttonPressed != BYTE_MAX) {
    inputstate = HANDLING_INPUT;
    OnButtonPressed(buttonPressed);
  }

}

byte GetCurrentPressedButton() {
  for (byte i = 0; i < 7; ++i) {
    byte pinNumber = i + 1;
    bool pressedDown = musicPlayer.GPIO_digitalRead(pinNumber) == HIGH;
    if (pressedDown) {
      return i;
    }
  }

  for (byte i = 0; i < pinArrayLength; ++i) {
    byte pinNumber = pinArray[i];
    bool pressedDown = digitalRead(pinNumber) == LOW;
    if (pressedDown) {
      return i + 7;
    }
  }

  if (digitalRead(PIN_REPLACE_FOR_11) == LOW) {
    return 11;
  }

  return BYTE_MAX;
}

void OnButtonPressed(byte buttonPressedId) {

  int next = 0;

  switch (buttonPressedId) {
    case BYTE_MAX:
      // illegal button id.
      return;
    case NEXT_BUTTON_ID:
      Serial.print("Play next from dir: ");
      Serial.print(currentDirIndex);
      Serial.println();
      next = 1;
      break;
    case PREV_BUTTON_ID:
      Serial.print("Play previous from dir: ");
      Serial.print(currentDirIndex);
      Serial.println();
      next = -1;
      break;
    default:
      bool shouldReset = currentDirIndex != buttonPressedId;
      if (shouldReset) {
        Reset(buttonPressedId, -1);
      }
      next = 1;
  }

  if (UpdateFileCursor(currentDirIndex, next)) {
    PlayNext();
  }

}

bool IsControlButton(byte buttonId) {
  return buttonId == NEXT_BUTTON_ID || buttonId == PREV_BUTTON_ID;
}

void Reset(int dirIndex, int fileIndex) {
  Serial.print("Resetting dir from ");
  Serial.print(currentDirIndex);
  Serial.print(" to " );
  Serial.print(dirIndex);
  Serial.println();

  Serial.print("Resetting currentFileIndex from ");
  Serial.print(currentFileIndex);
  Serial.print(" to " );
  Serial.print(fileIndex);
  Serial.println();

  currentDirIndex = dirIndex;
  currentFileIndex = fileIndex;
}

bool UpdateFileCursor(int dirIndex, int dir) {
  int next = currentFileIndex + dir;
  Serial.print("currentFileIndex: " );
  Serial.print(currentFileIndex);
  Serial.print(" + " );
  Serial.print(dir);
  Serial.print(" => " );
  Serial.println(next);
  int numberOfFiles = sumFilesPerFolderCache[dirIndex];
  Serial.print("numberOfFiles => " );
  Serial.println(numberOfFiles);
  if (next < 0 || next >= numberOfFiles)
  {
    return false;
  }
  currentFileIndex = next;
  return true;
}

void ContinuePlayingFromSession() {
  if (SD.exists(SESSION_TEXT_FILE_PATH)) {
    File textFile = SD.open(SESSION_TEXT_FILE_PATH, O_READ);
    String stored = "";
    while (textFile.available()) {
      stored += (char)textFile.read();
    }
    textFile.close();
    for (int i = 0; i < stored.length(); i++) {
      if (stored.substring(i, i + 1) == ",") {
        Serial.print("Stored");
        int storedDirIndex = stored.substring(0, i).toInt();
        int storedFileIndex = stored.substring(i + 1).toInt();
        Reset(storedDirIndex, storedFileIndex);
        if (UpdateFileCursor(storedDirIndex, 0)) {
          PlayNext();
        }

        break;
      }
    }
  } else {
    Serial.println("Couldn't find the session text file");
  }
}

void PlayNext() {

  Serial.println("Playing next file");

  String fileIndexAsString = String(currentFileIndex);
  String dirIndexAsString = String(currentDirIndex);
  //  String audioFile = "audio/" + dirIndexAsString + "/t_" + dirIndexAsString + "_" + fileIndexAsString + ".mp3";
  String audioFile = "audio/" + dirIndexAsString + "/" + fileIndexAsString + ".mp3";
  Serial.print("AUDIO ===>" );
  Serial.println(audioFile);
  // Length (with one extra character for the null terminator)
  int str_len = audioFile.length() + 1;
  //  // Prepare the character array (the buffer)
  char char_array[str_len];

  // Copy it over
  audioFile.toCharArray(char_array, str_len);

  // pausing it first prevents audible glitchy sounds.
  musicPlayer.pausePlaying(true);
  // now stop it.
  musicPlayer.stopPlaying();
  // we can only write to the SD card when the audio is stopped.
  // SD is either read or write.
  PersistCurrentSelectedData(dirIndexAsString, fileIndexAsString);
  Serial.println(char_array);
  musicPlayer.startPlayingFile(char_array);
}

void PersistCurrentSelectedData(String dirIndexAsString, String fileIndexAsString) {
  File sessionTextFile = SD.open(SESSION_TEXT_FILE_PATH, O_CREAT | O_TRUNC | O_WRITE);
  if (sessionTextFile) {
     Serial.print("STore ===>" );
  Serial.println(dirIndexAsString + ":"+fileIndexAsString);
    sessionTextFile.println(dirIndexAsString + "," + fileIndexAsString);
    sessionTextFile.close();
  }
}

void UpdateVolume() {
  // read the state of the volume potentiometer
  int pinValue = analogRead(VOLUME_PIN);

  // set the range of the volume from 0 to 100
  pinValue = map(pinValue, 0, 500, MAX_VOLUME, MIN_VOLUME + 5);
  //  Serial.println(pinValue);
  currentVolume = pinValue;
  musicPlayer.setVolume(currentVolume, currentVolume);
  //    delay(5); // delay in between reads for stability
}
