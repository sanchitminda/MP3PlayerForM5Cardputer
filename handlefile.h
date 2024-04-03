#include <SD.h>
String files[2000] =
{
};
int no_of_files = 0;
bool stop_scan = false;
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    // printf_log("Listing directory: %s\n", dirname);
    if(stop_scan){
      return;
    }
    File root = fs.open(dirname);
    if (!root) {
        // println_log("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        // println_log("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {

      bool scan = false;

        if (file.isDirectory()) {
            // Serial.print("  DIR : ");
            // println_log(file.name());
        M5Cardputer.update();
                M5Cardputer.Display.drawString("Do you want to scan(Y/N/C)",
                        1,
                        M5Cardputer.Display.height() / 2);
                        String filename = file.name();
                M5Cardputer.Display.drawString(filename,
                        1,
                        (M5Cardputer.Display.height()+20) / 2); 
        while(1){
          M5Cardputer.update();
          if(M5Cardputer.Keyboard.isChange()) break;
        } 


          if(M5Cardputer.Keyboard.isKeyPressed('y')){
            scan = true;
          }
          if(M5Cardputer.Keyboard.isKeyPressed('n')){
            scan = false;
          }
          if(M5Cardputer.Keyboard.isKeyPressed('c')){
            stop_scan = true;
            return;
          }

                                     
            if (levels && scan) {
                listDir(fs, file.path(), levels - 1);
            }
        } else {
          String filename = file.name();
          String filepath = file.path();
          if (filename.lastIndexOf(".mp3") > 0 && filepath.length()<256)     
          if(filepath != "")         
            {files[no_of_files++] = filepath;
                              M5Cardputer.Display.drawString("Current File:",
                        1,
                        M5Cardputer.Display.height()+40 / 2);
                        String filename = file.name();
                M5Cardputer.Display.drawString(filename,
                        1,
                        (M5Cardputer.Display.height()+60) / 2); 
            }
            
            // Serial.print("  FILE: ");
            // Serial.print(file.name());
            // Serial.print("  SIZE: ");
            // println_log(files[no_of_files]);
        }


        if(M5Cardputer.Keyboard.isChange()){
          if(M5Cardputer.Keyboard.isKeyPressed('s')){
            levels = 0;
            return;
          }
        }

        file = root.openNextFile();
    }
}
