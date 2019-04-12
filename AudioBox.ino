#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

/**
  --- information ---
  Here some example can be found of how to play the mp3s faster or slower
  https://forums.adafruit.com/viewtopic.php?f=31&t=107788
**/


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
#define MAX_VOLUME 15 // Volume is the lower the louder.
#define MIN_VOLUME 70 // Volume is the lower the louder.

#define READY_FOR_INPUT  1 // No button is pressed and we can receive a button input.
#define HANDLING_INPUT  2 // For making sure we only handle one button input.

Adafruit_VS1053_FilePlayer musicPlayer =  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

const byte PREV_BUTTON_ID = 10; // The number of the button for playing the previous.
const byte NEXT_BUTTON_ID = 11; // The number of the button for playing the next.
const byte BYTE_MAX = 255; // To check against an unset byte type.
const char sessionTextfilePath[] = "store.txt";
const char introSoundPath[] = "intro.mp3";
const char audioBaseFolderPath[] = "audio/";
const char filecountTextfilePath[] = "audio/nfo.txt";
const byte pinArray[4] = {2, 5, 8, 9};
const byte pinArrayLength = 4;

int sumFilesPerFolderCache[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int currentVolume = MIN_VOLUME;
int currentFileIndex = -1;
byte currentDirIndex = BYTE_MAX;
byte inputState = READY_FOR_INPUT;
bool startingUp = true;
bool resetAtStart = false;
int16_t randomPlayableDirBitMask = 0;

int randomValue = 0;
int startupPlayedIndex = BYTE_MAX;

void setup() {

//  Serial.begin(9600);

  if (! musicPlayer.begin()) { // initialise the music player
    //    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1);
  }

  if (!SD.begin(CARDCS)) {
    //    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }

  SetupPins();
  BuildPlaylistIndex();
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);

  resetAtStart = !digitalRead(PIN_REPLACE_FOR_11) && !digitalRead(9);

  if (!resetAtStart) {
    musicPlayer.setVolume(MAX_VOLUME, MAX_VOLUME);
    musicPlayer.playFullFile(introSoundPath);
  } else {
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

void BuildPlaylistIndex() {
  //  Serial.println(F("BuildPlaylistIndex"));
  if (SD.exists(filecountTextfilePath)) {
    //    Serial.println(F("Found the nfo.txt file"));
    File textFile = SD.open(filecountTextfilePath, O_READ);
    while (textFile.available()) {
      String readString = textFile.readStringUntil('\r');
      char c[30];
      readString.toCharArray(c, sizeof(c));
      int ipos = 0;
      // Get the first token from the string
      char *tok = strtok(c, ",");
      // Keep going until we run out of tokens
      while (tok) {
        // Don't overflow your target array
        if (ipos < 10) {
          int value = atoi(tok);
          Serial.println(value);
          sumFilesPerFolderCache[ipos++] = value;
        }
        // Get the next token from the string - note the use of NULL
        // instead of the string in this case - that tells it to carry
        // on from where it left off.
        tok = strtok(NULL, ",");
      }

      randomPlayableDirBitMask = textFile.readStringUntil('\r').toInt();

      textFile.close();
    }
  }
}

void loop() {
  if (startingUp && !resetAtStart) {
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
    PlayCurrent();
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
      delay(250);
      if (!musicPlayer.readyForData())
        return;
      PlayNext();
    }
  }
  delay(1);
}

void UpdateVolume() {
  int pinValue = analogRead(VOLUME_PIN);
  currentVolume = map(pinValue, 0, 688, MIN_VOLUME, MAX_VOLUME);
  musicPlayer.setVolume(currentVolume, currentVolume);
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
  switch (buttonPressedId) {
    case NEXT_BUTTON_ID:
      PlayNext();
      break;
    case PREV_BUTTON_ID:
      PlayPrevious();
      break;
    default:
      bool directorySelectionChanged = currentDirIndex != buttonPressedId;
      if (directorySelectionChanged) {
        Reset(buttonPressedId, 0);
        PlayCurrent();
      } else {
        PlayNext();
      }
  }
}

void ContinuePlayingFromSession() {

  int storedDirIndex = 0;
  int storedFileIndex = 0;

  if (SD.exists(sessionTextfilePath))
  {
    File textFile = SD.open(sessionTextfilePath, FILE_READ);
    while (textFile.available())
    {
      storedDirIndex = textFile.readStringUntil('\n').toInt();
      storedFileIndex = textFile.readStringUntil('\n').toInt();
    }
    textFile.close();

    bool fileIsCorrupted = storedDirIndex > 9 || storedDirIndex == BYTE_MAX;
    
    if (fileIsCorrupted)
    {
      Serial.println("File could not be read");
      storedDirIndex = 0;
      storedFileIndex = 0;
      SD.remove(sessionTextfilePath);
    }
  }

  Reset(storedDirIndex, storedFileIndex);
  startupPlayedIndex = storedFileIndex;
  PlayCurrent();
}

void Reset(int dirIndex, int fileIndex) {
  currentDirIndex = dirIndex;
  currentFileIndex = fileIndex;
  startupPlayedIndex = BYTE_MAX;
    randomSeed(millis());
    randomValue = random();
}

void PlayPrevious() {
  currentFileIndex--;
  PlayCurrent();
}

void PlayNext() {
  currentFileIndex++;
  PlayCurrent();
}

void PlayCurrent() {
  int fileIndexToPlay;

  if (IsRandomDir(currentDirIndex) && startupPlayedIndex != currentFileIndex) {
    randomSeed(currentFileIndex + randomValue);
    fileIndexToPlay = random(0, sumFilesPerFolderCache[currentDirIndex]);
  } else {
    fileIndexToPlay = mod(currentFileIndex,sumFilesPerFolderCache[currentDirIndex]);
  }
  
  Play(currentDirIndex, fileIndexToPlay, true);
}

bool IsRandomDir(byte dirIndex){
    return bitRead(randomPlayableDirBitMask, dirIndex) == 1;
}

int mod( int x, int y ){
   return x < 0 ? ( (x+1) % y)+ y-1 : x % y;
}

void Play(int dirIndexToPlay, int fileIndexToPlay, bool saveToSD) {

  String fileIndexAsString = String(fileIndexToPlay);
  String dirIndexAsString = String(dirIndexToPlay);
  String audioFile = audioBaseFolderPath + dirIndexAsString + "/" + fileIndexAsString + ".mp3";
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
  if (saveToSD) {
    PersistCurrentSelectedData(dirIndexAsString, fileIndexAsString);
  }
  musicPlayer.startPlayingFile(char_array);
}

void PersistCurrentSelectedData(String dirIndexAsString, String fileIndexAsString) {
  File sessionTextFile = SD.open(sessionTextfilePath, O_CREAT | O_TRUNC | O_WRITE);
  if (sessionTextFile) {
    sessionTextFile.println(dirIndexAsString);
    sessionTextFile.println(fileIndexAsString);
    sessionTextFile.close();
  }
}
