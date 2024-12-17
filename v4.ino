#include "FS.h"
#include "LittleFS.h"
#include "SD.h"
#include <ArduinoJson.h>
#include <AnimatedGIF.h> 
#include <TFT_eSPI.h> 
#include "SPI.h"

// some options
#define FORMAT_LittleFS_IF_FAILED true

// define screen
TFT_eSPI tft = TFT_eSPI(); 

// gif config data
String gifname;
int gifidx;
AnimatedGIF gif;
File gifFile;


// sd card read aux vars
String *fileslist;
int filesnumber = 0;
const char *filename = "/gif.gif";

//physical button
#define physical_button_inc 5
#define physical_button_sub 6
#define physical_button_rand 7

bool current_state_inc = false;
bool last_state_inc = false;
bool current_state_sub = false;
bool last_state_sub = false;
bool current_state_rand = false;
bool last_state_rand = false;
bool rand_trigger = false;
bool inc_trigger = false;
bool sub_trigger = false;
//-------------------------------------------------Setup---------------------------------------
void setup(){

  Serial.begin(115200);
  pinMode(physical_button_inc, INPUT_PULLUP);
  pinMode(physical_button_rand, INPUT_PULLUP);
  pinMode(physical_button_sub, INPUT_PULLUP);
  
  // Initialize SPIFFS
  Serial.flush();
  Serial.println("Initialize SPIFFS...");
  if (!LittleFS.begin(true)){
    Serial.println("SPIFFS initialization failed!");
    while (true){
      Serial.println("Cound't start SPIFFS. PLease manual reset the ESP.");
      delay(2000);
    }
  }

  // Check if files in SPIFFS exist
  if (!LittleFS.exists("/gifconfig.json")){
    Serial.println("File gifconfig.json doesn't exist.");
    gifidx = 0;
    ReadSDCard();
  }
  else{
    Serial.println("File gifconfig.json found");
    readconfig("/gifconfig.json");
    
    Serial.print("Gif id: ");
    Serial.println(gifidx);
    Serial.print("Gif name: ");
    Serial.println(gifname);

    // -> check for gif
    if (!LittleFS.exists(filename)){
      Serial.println("File gif.h doesn't exist.");
      ReadSDCard();
    }
    else{
      Serial.println("File gif.gif found");
      
      // setup screen
      tft.begin();
      tft.setRotation(7); // Adjust Rotation of your screen (0-3)
      tft.fillScreen(TFT_BLACK);

      Serial.println("Starting animation...");
      gif.begin(BIG_ENDIAN_PIXELS);
    }
  }
}

//----------------------Loop----------------------------------------------

void loop()
{
  if (gif.open(filename, fileOpen, fileClose, fileRead, fileSeek, GIFDraw))
  {
    tft.startWrite(); // The TFT chip slect is locked low
    while (gif.playFrame(true, NULL))
    {
      if(ButtonPressed_inc()){
        gif.close();
        tft.endWrite(); // Release TFT chip select for the SD Card Reader
        // delay(50);
        SPI.end();
        inc_trigger = true;
        ReadSDCard();
      }

      if(ButtonPressed_rand()){
        gif.close();
        tft.endWrite(); // Release TFT chip select for the SD Card Reader
        // delay(50);
        SPI.end();
        rand_trigger = true;
        ReadSDCard();
      }

      if(ButtonPressed_sub()){
        gif.close();
        tft.endWrite(); // Release TFT chip select for the SD Card Reader
        // delay(50);
        SPI.end();
        sub_trigger = true;
        ReadSDCard();
      }
    }
    gif.close();
    tft.endWrite(); // Release TFT chip select for the SD Card Reader
  }
}

// Callbacks for file operations for the Animated GIF Lobrary
void *fileOpen(const char *filename, int32_t *pFileSize)
{
  gifFile = LittleFS.open(filename, FILE_READ);
  *pFileSize = gifFile.size();
  if (!gifFile)
  {
    Serial.println("Failed to open GIF file from SPIFFS!");
    delay(500);
  }
  return &gifFile;
}

void fileClose(void *pHandle)
{
  gifFile.close();
}

int32_t fileRead(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
  int32_t iBytesRead;
  iBytesRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos;
  if (iBytesRead <= 0)
    return 0;

  gifFile.seek(pFile->iPos);
  int32_t bytesRead = gifFile.read(pBuf, iLen);
  pFile->iPos += iBytesRead;

  return bytesRead;
}

int32_t fileSeek(GIFFILE *pFile, int32_t iPosition)
{
  if (iPosition < 0)
    iPosition = 0;
  else if (iPosition >= pFile->iSize)
    iPosition = pFile->iSize - 1;
  pFile->iPos = iPosition;
  gifFile.seek(pFile->iPos);
  return iPosition;
}



//--------------------Save next file in SPIFFS-----------------------------------
void ReadSDCard(){
  // Initialize SD

  SPI.begin(10, 21, 20, 9);
  if(!SD.begin(9)){
    Serial.println("Card Mount Failed");
    return;
  }

  //list files in sd card
  listfiles();
  if (inc_trigger){
    Serial.println("Activating inc method");
    delay(10);
    gifidx++;
    if (gifidx >= filesnumber){
      gifidx=0;
      Serial.println("Reseting gif counter");
    }
  } else if(rand_trigger){
    gifidx = random(0,filesnumber);
    Serial.println("Activating rand method");
    delay(10);
    Serial.print("Random number selected: ");Serial.println(gifidx);
  } else if (sub_trigger){
    Serial.println("Activating sub method");
    delay(10);
    if (gifidx == 0){
      gifidx=filesnumber-1;
    } else{
      gifidx--;
    }
  }


  // from sd to spiffs
  Serial.print("Importing ");Serial.print(fileslist[gifidx]);Serial.println(" to SPIFFS.");
  File sourceFile = SD.open("/" + fileslist[gifidx]);
  if (!sourceFile){Serial.println("Cound't open SD file!");}
  Serial.println(LittleFS.exists(filename));
  File destFile = LittleFS.open(filename, FILE_WRITE);
  
  static uint8_t buf[512]; 
  while(sourceFile.read( buf, 512) ) {
    destFile.write( buf, 512 );
  }
  destFile.close();
  sourceFile.close();
  Serial.println("Import completed!");

  // write config file
  if (saveconfig("/gifconfig.json")){
    Serial.println("Config file saved in SPIFFS");
  }
  Serial.println("ESP will restart...");
  Serial.println("---------------------------------------");
  ESP.restart();
}

//--------------------List files from SD card-----------------------------------
void listfiles(){ 
  File root = SD.open("/");

  if(!root){
    Serial.println("Failed to open directory");
    return;
  }

  if(!root.isDirectory()){
    Serial.println("Not a directory");
    return;
  }

  // count number of files
  File file = root.openNextFile();
  bool firstrun = true;
  filesnumber = 0;
  while(file){
    if (!firstrun){
      filesnumber++;
    }
    else{
      firstrun = false;
    }
    file = root.openNextFile();
  }

  root.rewindDirectory();
  // create files array
  fileslist = new String[filesnumber];
  file = root.openNextFile();
  firstrun = true;
  int i = 0;
  while(i < filesnumber){
    if (!firstrun){
      fileslist[i] = file.name();
      i++;
    }
    else{
      firstrun = false;
    }
    file = root.openNextFile();
  }
  file.close();
}

//-----------------------------save config file----------------------------------
bool saveconfig(String file_name){
  StaticJsonDocument<1024> doc;

  doc["gifidx"] = gifidx;
  doc["gifname"] = fileslist[gifidx];

  // write config file
  String tmp = "";
  serializeJson(doc,tmp);
  // return writeFile(SPIFFS, file_name, tmp);
  File giffile = LittleFS.open(file_name,FILE_WRITE);
  if (!giffile){
    Serial.println("Couldn't open the file to write...");
  }
  Serial.print("Saving config file in "); Serial.println(file_name);
  if(giffile.print(tmp)){
    Serial.println("Config file written!");
  }else{
    Serial.println("Config file write failed");
  }
  giffile.close();
  return true;
}

//-----------------------------read config file-------------------------------
bool readconfig(String filename){
  File file = LittleFS.open(filename);
  String fileText = "";
  while (file.available()){
    fileText = file.readString();
  }
  file.close();

  int conf_file_len = fileText.length();

  if(conf_file_len>1024){
    Serial.println("Config File too large!");
    return false;
  }

  StaticJsonDocument<1024> doc;
  auto error = deserializeJson(doc,fileText);
  if (error){
    Serial.println("Error interpreting config file");
    return false;
  }

  const int _gifidx = doc["gifidx"];
  const String _gifname = doc["gifname"];

  Serial.println(_gifname);

  gifidx = _gifidx;
  gifname = _gifname;
  return true;
}

//------------------------------read config file............................
String readFile(fs::FS &fs, String filename){
  File file = fs.open(filename);
  String fileText = "";
  while (file.available()){
    fileText = file.readString();
  }
  file.close();
  return fileText;
}


// ----------------------------- if inc button pressed-------------------------------
bool ButtonPressed_inc(){
  int current_state_inc = digitalRead(physical_button_inc);
  // Serial.println(current_state);
  if (current_state_inc != last_state_inc){
    if (!current_state_inc){
      Serial.println("inc button pressed");
      last_state_inc = current_state_inc;
      return true;
    }
    else{
      Serial.println("inc button released");
    }
  }

  last_state_inc = current_state_inc; 
  return false; // No change in state
}

// ----------------------------- if rand button pressed-------------------------------
bool ButtonPressed_rand(){
  int current_state_rand = digitalRead(physical_button_rand);
  // Serial.println(current_state);
  if (current_state_rand != last_state_rand){
    if (!current_state_rand){
      Serial.println("rand button pressed");
      last_state_rand = current_state_rand;
      return true;
    }
    else{
      Serial.println("rand button released");
    }
  }

  last_state_rand = current_state_rand; 
  return false; // No change in state
}

// ----------------------------- if sub button pressed-------------------------------
bool ButtonPressed_sub(){
  int current_state_sub = digitalRead(physical_button_sub);
  // Serial.println(current_state);
  if (current_state_sub != last_state_sub){
    if (!current_state_sub){
      Serial.println("sub button pressed");
      last_state_sub = current_state_sub;
      return true;
    }
    else{
      Serial.println("sub button released");
    }
  }

  last_state_sub = current_state_sub; 
  return false; // No change in state
}
//-------------------------------------------END--------------------------------------