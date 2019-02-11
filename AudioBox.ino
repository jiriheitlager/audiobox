#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <avr/pgmspace.h>

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

#define VOLUME_PIN A1 // The volume pin which is connected to the potentiometer.
#define PIN_REPLACE_FOR_11 A0 // The volume pin which is connected to the potentiometer.

#define BYTE_MAX 255 // To check against an unset byte type.

#define READY_FOR_INPUT  1 // No button is pressed and we can receive a button input.
#define HANDLING_INPUT  2 // For making sure we only handle one button input.

Adafruit_VS1053_FilePlayer musicPlayer =  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

// Volume bounds
const PROGMEM char maxVolumeBound = 5; // Volume is the lower the louder.
const PROGMEM char minVolumeBound = 100; // Volume is the lower the louder.

// forward and backwards button ids //
const PROGMEM char previousButtonId = 10; // The number of the button for playing the previous.
const PROGMEM char nextButtonId = 11; // The number of the button for playing the next.

// const strings
const PROGMEM char sessionTextfilePath[] = "store.txt";
const PROGMEM char introSoundPath[] = "intro.mp3";
const PROGMEM char audioBaseFolderPath[] = "audio/";

// arduino pins mapping for buttons with id 8,9,10,11 (12 is mapped to A0)
const PROGMEM byte pinArray[] = {2, 5, 8, 9};
const PROGMEM byte pinArrayLength = 4;

byte currentVolume = minVolumeBound;
byte currentFileIndex = -1;
byte currentDirIndex = BYTE_MAX;
byte inputState = READY_FOR_INPUT;
bool startingUp = true;
bool resetAtStart = false;

/*
   //https://forums.adafruit.com/viewtopic.php?f=31&t=107788
           // increase play speed
                musicPlayer.sciWrite(VS1053_REG_WRAMADDR, para_playSpeed);
                musicPlayer.sciWrite(VS1053_REG_WRAM, 3);
                //Serial.println("increase speed");
*/
void setup() {

  Serial.begin(9600);

  if (! musicPlayer.begin()) { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1);
  }

  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }

  SetupPins();
  BuildPlaylistIndex();
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);

  resetAtStart = !digitalRead(PIN_REPLACE_FOR_11) && !digitalRead(9);
  //  Serial.print("Boot value ");
  //  Serial.println(resetAtStart );
  if (!resetAtStart) {
    musicPlayer.playFullFile(introSoundPath);
  } else {
    musicPlayer.sineTest(0x44, 600);
  }
}

void SetupPins() {

  pinMode(PIN_REPLACE_FOR_11, INPUT_PULLUP);

  for (byte count = 0; count < pinArrayLength; count++) {
    pinMode(pinArray[count], INPUT_PULLUP);
  }

  for (byte i = 0; i < 7; i++) {
    musicPlayer.GPIO_digitalWrite(i, HIGH);
  }
}

// I could consider storing this data in a file on the SD, so we dont need to build the index at startup.
// It would be faster to load the file and parse it. It is also feasible because we create the data on the SD ourselves.
byte sumFilesPerFolderCache[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
void BuildPlaylistIndex() {

  for (byte i = 0; i < 10; i++) {
    String path =  audioBaseFolderPath + String(i);

    File audioDirectory = SD.open(path, O_READ);
    audioDirectory.rewindDirectory();
    byte sumFiles = 0;
    while ( true )
    {
      File entry =  audioDirectory.openNextFile();
      if ( entry )
      {
        String file_name = entry.name();
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
    //    Serial.print("[Cache files directory] ");
    //    Serial.print(i);
    //    Serial.print(" has ");
    //    Serial.print(sumFiles);
    //    Serial.print(" files ");
    //    Serial.println();
    sumFilesPerFolderCache[i] = sumFiles;
  }
}

void loop() {
  if (startingUp && !resetAtStart) {
    Serial.println(F("Intro sound playing"));
    if (musicPlayer.readyForData()) {
    Serial.println(F("Intro sound done"));
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

  byte buttonPressed = GetCurrentPressedButton();
  if (inputState == HANDLING_INPUT && buttonPressed == BYTE_MAX) {
    inputState = READY_FOR_INPUT;
  } else if (inputState == READY_FOR_INPUT && buttonPressed != BYTE_MAX) {
    inputState = HANDLING_INPUT;
    OnButtonPressed(buttonPressed);
  }

  if (inputState == READY_FOR_INPUT) {
    if (musicPlayer.readyForData()) {
      Serial.println(F("[Auto continue]"));
      // the current track has stopped playing so we continue
      if (IncrementFileIterator(1)) {
        PlayNext();
      } else {
        Reset(currentDirIndex, 0);
        PlayNext();
      }
    }
  }
}

byte GetCurrentPressedButton() {
  for (byte i = 0; i < 7; ++i) {
    if (musicPlayer.GPIO_digitalRead(i + 1) == HIGH) {
      return i;
    }
  }

  for (byte i = 0; i < pinArrayLength; ++i) {
    if (digitalRead(pinArray[i]) == LOW) {
      return i + 7;
    }
  }

  if (digitalRead(PIN_REPLACE_FOR_11) == LOW) {
    return 11;
  }

  return BYTE_MAX;
}

void OnButtonPressed(byte buttonPressedId) {

  byte next = 0;

  switch (buttonPressedId) {
    case nextButtonId:
      //      Serial.print("Play next from dir: ");
      //      Serial.print(currentDirIndex);
      //      Serial.println();
      next = 1;
      break;
    case previousButtonId:
      //      Serial.print("Play previous from dir: ");
      //      Serial.print(currentDirIndex);
      //      Serial.println();
      next = -1;
      break;
    default:
      bool shouldReset = currentDirIndex != buttonPressedId;
      if (shouldReset) {
        Reset(buttonPressedId, -1);
      }
      next = 1;
  }

  if (IncrementFileIterator(next)) {
    PlayNext();
  }
}

void Reset(byte dirIndex, byte fileIndex) {
  //  Serial.print("Resetting dir from ");
  //  Serial.print(currentDirIndex);
  //  Serial.print(" to " );
  //  Serial.print(dirIndex);
  //  Serial.println();
  //
  //  Serial.print("Resetting currentFileIndex from ");
  //  Serial.print(currentFileIndex);
  //  Serial.print(" to " );
  //  Serial.print(fileIndex);
  //  Serial.println();

  currentDirIndex = dirIndex;
  currentFileIndex = fileIndex;
}

bool IncrementFileIterator(byte direction) {
  byte nextFileIndex = currentFileIndex + direction;
  //  Serial.print("[increment File iterator] : from " );
  //  Serial.print(currentFileIndex);
  //  Serial.print(" => " );
  //  Serial.println(nextFileIndex);
  byte numberOfFiles = sumFilesPerFolderCache[currentDirIndex];
  //  Serial.print("[NumberOfFiles for the current dir] ");
  //  Serial.println(numberOfFiles);
  if (nextFileIndex < 0 || nextFileIndex >= numberOfFiles)
  {
    return false;
  }
  currentFileIndex = nextFileIndex;
  return true;
}

void ContinuePlayingFromSession() {
  Reset(0, 0);
  //https://forums.adafruit.com/viewtopic.php?f=31&t=107788

  //  Serial.print("[Continue to play from SD stored] " );
  if (SD.exists(sessionTextfilePath)) {
    File textFile = SD.open(sessionTextfilePath, O_READ);
    String stored = ""; //TODO string.reserve maybe ?
    while (textFile.available()) {
      stored += (char)textFile.read();
    }
    textFile.close();

    Serial.print(F("[Stored data] "));
    Serial.println(stored.length());

    for (byte i = 0; i < stored.length(); i++) {
      if (stored.substring(i, i + 1) == ",") {
        byte storedDirIndex = stored.substring(0, i).toInt();
        byte storedFileIndex = stored.substring(i + 1).toInt();

        if (storedDirIndex > 9 || storedDirIndex == BYTE_MAX || storedFileIndex < 0 || storedFileIndex > sumFilesPerFolderCache[storedFileIndex])
        {
          Serial.print(F("[Error in reading the stored data. Deleted the store textfile] "));
          SD.remove(sessionTextfilePath);
        } else {
          Reset(storedDirIndex, storedFileIndex);
        }
        break;
      }
    }
  }

  if (IncrementFileIterator(0)) {
    PlayNext();
  }
}

void PlayNext() {
  // TODO: see if string.reserver can be used to manipulate the path string
  String fileIndexAsString = String(currentFileIndex);
  String dirIndexAsString = String(currentDirIndex);
  String audioFile = "audio/" + dirIndexAsString + "/" + fileIndexAsString + ".mp3";
  //  Serial.print("[Playing as next] " );
  //  Serial.println(audioFile);
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
  File sessionTextFile = SD.open(sessionTextfilePath, O_CREAT | O_TRUNC | O_WRITE);
  if (sessionTextFile) {
    //    Serial.print("[Storing to SD file] " );
    //    Serial.println(dirIndexAsString + ":" + fileIndexAsString);
    sessionTextFile.println(dirIndexAsString + "," + fileIndexAsString);
    sessionTextFile.close();
  }
}

void UpdateVolume() {
  // read the state of the volume potentiometer
  byte pinValue = analogRead(VOLUME_PIN);

  // set the range of the volume from 0 to 100
  // TODO: when the potentiometer is turned down, it never really goes to silent.
  currentVolume = map(pinValue, 0, 500, maxVolumeBound, minVolumeBound + 5);
  //  musicPlayer.setVolume(currentVolume, currentVolume);

  musicPlayer.setVolume(20  , 20);

}
