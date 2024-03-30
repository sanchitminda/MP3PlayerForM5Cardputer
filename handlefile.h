#include <SD.h>
String files[2000] =
{
};
int no_of_files = 0;
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    // printf_log("Listing directory: %s\n", dirname);

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
        if (file.isDirectory()) {
            // Serial.print("  DIR : ");
            // println_log(file.name());

                                   
            if (levels) {
                listDir(fs, file.path(), levels - 1);
            }
        } else {
          String filename = file.name();
          String filepath = file.path();
          if (filename.lastIndexOf(".mp3") > 0 )     
          if(filepath != "")         
            files[no_of_files++] = filepath;
            
            // Serial.print("  FILE: ");
            // Serial.print(file.name());
            // Serial.print("  SIZE: ");
            // println_log(files[no_of_files]);
        }
        file = root.openNextFile();
    }
}
