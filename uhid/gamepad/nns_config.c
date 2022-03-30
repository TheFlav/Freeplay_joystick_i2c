/*
* NNS configuration file handler
*/

#include "driver_debug_print.h"
#include "nns_config.h"

int config_sum (cfg_vars_t* cfg, unsigned int cfg_size){ //pseudo checksum for config build
    int ret = 0;
    for (unsigned int i = 0; i < cfg_size; i++){ret += strlen(cfg[i].name) + (cfg[i].type + 1)*2 + i*4;}
    return ret;
}

int config_search_name (cfg_vars_t* cfg, unsigned int cfg_size, char *value, bool skipNl){ //search in cfg_vars struct, return index if found, -1 if not
    char *rowPtr;
    for (unsigned int i = 0; i < cfg_size; i++) {
        char tmpVar [strlen(cfg[i].name)+1]; strcpy (tmpVar, cfg[i].name);
        if (skipNl && tmpVar[0]=='\n') {rowPtr = tmpVar + 1;} else {rowPtr = tmpVar;}
        if (strcmp (rowPtr, value) == 0) {return i;}
    }
    return -1;
}

int config_save (cfg_vars_t* cfg, unsigned int cfg_size, char* filename, int uid, int gid, bool reset){ //save config file
    if (reset) {if(remove(filename) != 0) {print_stderr("failed to delete '%s'\n", filename);}}
    FILE *filehandle = fopen(filename, "wb");
    if (filehandle != NULL) {
        char strBuffer [4096], strBuffer1 [33];
        for (unsigned int i = 0; i < cfg_size; i++) {
            int tmpType = cfg[i].type;
            if (tmpType == 0) {fprintf (filehandle, "%s=%d;", cfg[i].name, *(int*)cfg[i].ptr); //int
            } else if (tmpType == 1) {fprintf (filehandle, "%s=%u;", cfg[i].name, *(unsigned int*)cfg[i].ptr); //unsigned int
            } else if (tmpType == 2) {fprintf (filehandle, "%s=%f;", cfg[i].name, *(float*)cfg[i].ptr); //float
            } else if (tmpType == 3) {fprintf (filehandle, "%s=%lf;", cfg[i].name, *(double*)cfg[i].ptr); //double
            } else if (tmpType == 4) {fprintf (filehandle, "%s=%d;", cfg[i].name, (*(bool*)cfg[i].ptr)?1:0); //bool
            } else if (tmpType == 5) { //int array, output format: var=%d,%d,%d,...;
                int arrSize = *(int*)((int*)cfg[i].ptr)[1];
                sprintf (strBuffer, "%s=", cfg[i].name); int strBufferSize = strlen(strBuffer);
                for (unsigned int j = 0; j < arrSize; j++) {strBufferSize += sprintf (strBuffer1, "%d,", ((int*)((int*)cfg[i].ptr)[0])[j]); strcat (strBuffer, strBuffer1);} *(strBuffer+strBufferSize-1) = '\0';
                fprintf (filehandle, "%s;", strBuffer);
            } else if (tmpType >= 6 && tmpType < 9) { //hex8-32
                int ind = 2; for(int j=0; j<tmpType-6; j++){ind*=2;}
                sprintf (strBuffer, "%%s=0x%%0%dX;", ind); //build that way to limit var size
                fprintf (filehandle, strBuffer, cfg[i].name, *(int*)cfg[i].ptr);
            } else if (tmpType >= 9 && tmpType < 12) { //bin8-32 (itoa bypass)
                int ind = 8; for(int j=0; j<tmpType-9; j++){ind*=2;}
                for(int j = 0; j < ind; j++){strBuffer[ind-j-1] = ((*(int*)cfg[i].ptr >> j) & 0b1) +'0';} strBuffer[ind]='\0';
                fprintf (filehandle, "%s=%s;", cfg[i].name, strBuffer);
            }
            if(strlen(cfg[i].desc) > 0){fprintf (filehandle, " //%s\n", cfg[i].desc); //add comments
            }else{fprintf (filehandle, "\n");}
        }

        fprintf (filehandle, "\ncfg_version=0x%X; //Change value to force resync of configuration file.", (int)config_sum (cfg, cfg_size)); //write config version
        
        fclose(filehandle);
        print_stdout("%s: %s\n", reset ? "config reset successfully" : "config saved successfully", filename);

        if (uid !=-1 && gid != -1){ //config file owner
            struct stat file_stat = {0}; bool failed = false;
            if (stat(filename, &file_stat) == 0){
                if (uid !=-1 && (file_stat.st_uid != uid || gid !=-1) && file_stat.st_gid != gid) {
                    if (chown(filename, (uid_t) uid, (gid_t) gid) < 0){failed = true;
                    } else {print_stdout("%s owner changed successfully (uid:%d, gid:%d)\n", filename, uid, gid);}
                }
            } else {failed = true;}
            if (failed){print_stderr("changing %s owner failed with errno:%d (%m)\n", filename, errno); return -errno;}
        }

        return 0;
    } else {print_stderr("writing %s failed with errno:%d (%m)\n", filename, errno);}
    return -errno;
}

int config_set (cfg_vars_t* cfg, unsigned int cfg_size, char* filename, int uid, int gid, bool readcfg, char* var_value){ //update var in config file, var_value format: var=value
    if (readcfg){config_parse(cfg, cfg_size, filename, uid, gid);} //parse config file, create if needed
    int ret; char *tmpPtr = strchr(var_value, '='); //'=' char position
    if (tmpPtr != NULL){ //contain '='
        *tmpPtr='\0'; int tmpVarSize = strlen(var_value), tmpValSize = strlen(tmpPtr+1); //var and val sizes
        char tmpVar [tmpVarSize+1]; strcpy (tmpVar, var_value); //extract var
        char tmpVal [tmpValSize+1]; strcpy (tmpVal, tmpPtr + 1); //extract val
        int tmpIndex = config_search_name (cfg, cfg_size, tmpVar, true); //var in config array
        if (tmpIndex != -1) { //found in config array
            if (config_type_parse (cfg, cfg_size, tmpIndex, cfg[tmpIndex].type, tmpVar, tmpVal)){ //parse config var with specific type
                print_stderr("Config '%s' set to '%s'\n", tmpVar, tmpVal);
                return config_save(cfg, cfg_size, filename, uid, gid, false); //save new config
            } else {print_stderr("FATAL: Config: failed to set '%s' to '%s'\n", tmpVar, tmpVal); ret = -EPERM;}
        } else {print_stderr("FATAL: Config: '%s' not in config set\n", tmpVar); ret = -EPERM;}
    } else {print_stderr("FATAL: Config: invalid var_value argument, format: VAR=VALUE\n"); ret = -EPERM;}
    return ret;
}

bool config_type_parse (cfg_vars_t* cfg, unsigned int cfg_size, int index, int type, char* var, char* value){ //parse config var with specific type
    if (index < 0 || index > cfg_size){if(debug){print_stderr("DEBUG: invalid index(%d), limit:0-%d\n", index, cfg_size);} return false;}
    char strTmpBuffer [4096]; //string buffer
    int tmpValSize = strlen(value);
    if (type == 0) {
        *(int*)cfg[index].ptr = atoi (value); //int
        if(debug){print_stderr("DEBUG: %s=%d (file:%s)(int:%d)\n", var, *(int*)cfg[index].ptr, value, type);}
    } else if (type == 1) {
        *(unsigned int*)cfg[index].ptr = atoi (value); //unsigned int
        if(debug){print_stderr("DEBUG: %s=%u (file:%s)(uint:%d)\n", var, *(unsigned int*)cfg[index].ptr, value, type);}
    } else if (type == 2) {
        *(float*)cfg[index].ptr = atof (value); //float
        if(debug){print_stderr("DEBUG: %s=%f (file:%s)(float:%d)\n", var, *(float*)cfg[index].ptr, value, type);}
    } else if (type == 3) {
        *(double*)cfg[index].ptr = atof (value); //double
        if(debug){print_stderr("DEBUG: %s=%lf (file:%s)(double:%d)\n", var, *(double*)cfg[index].ptr, value, type);}
    } else if (type == 4) {
        *(bool*)cfg[index].ptr = (atoi (value) > 0)?true:false; //bool
        if(debug){print_stderr("DEBUG: %s=%d (file:%s)(bool:%d)\n", var, *(bool*)cfg[index].ptr?1:0, value, type);}
    } else if (type == 5) { //int array, input format: var=%d,%d,%d,...;
        int arrSize = *(int*)((int*)cfg[index].ptr)[1]; int j = 0;
        char tmpVal1 [tmpValSize+1]; strcpy(tmpVal1, value);
        char *tmpPtr2 = strtok(tmpVal1, ",");
        while (tmpPtr2 != NULL) {
            if (j < arrSize) {((int*)((int*)cfg[index].ptr)[0])[j] = atoi(tmpPtr2);} //no overflow
            j++; tmpPtr2 = strtok(NULL, ","); //next element
        }

        if(debug){
            if (j!=arrSize) {print_stderr("DEBUG: Warning var '%s' elements count mismatch, should have %d but has %d\n", var, arrSize, j);}
            char strBuffer1 [4096]={'\0'}; strTmpBuffer[0]='\0'; int strBufferSize = 0;
            for (j = 0; j < arrSize; j++) {strBufferSize += sprintf (strBuffer1, "%d,", ((int*)((int*)cfg[index].ptr)[0])[j]); strcat (strTmpBuffer, strBuffer1);} *(strTmpBuffer+strBufferSize-1) = '\0';
            print_stderr("DEBUG: %s=%s (file:%s)(int array:%d)\n", var, strTmpBuffer, value, type);
        }
    } else if (type >= 6 && type < 9) { //hex8-32
        int ind = 2; for(int j=0; j<type-6; j++){ind*=2;}
        char *tmpPtr2 = strchr(value,'x');
        if(tmpPtr2!=NULL){
            sprintf(strTmpBuffer, "0x%%0%dX", ind);
            sscanf(value, strTmpBuffer, cfg[index].ptr);
        }else{
            if(debug){print_stderr("DEBUG: Warning var '%s' should have a hex value, assumed a int\n", var);}
            *(int*)cfg[index].ptr = atoi (value);
        }

        if(debug){print_stderr("DEBUG: %s=0x%X(%d) (file:%s)(hex:%d)\n", var, *(int*)cfg[index].ptr, *(int*)cfg[index].ptr, value, type);}
    } else if (type >= 9 && type < 12) { //bin8-32
        int ind = 8; for(int j=0; j<type-9; j++){ind*=2;}
        if (debug && tmpValSize!=ind) {print_stderr("DEBUG: Warning var '%s' value lenght mismatch, needs %d but has %d\n", var, ind, tmpValSize);}
        
        int tmp = 0; for(int j = 0; j < ind; j++){if (j<tmpValSize) {if(value[tmpValSize-j-1] > 0+'0'){tmp ^= 1U << j;}}}
        *(int*)cfg[index].ptr = tmp;

        if(debug){
            for(int j = 0; j < ind; j++){strTmpBuffer[ind-j-1] = ((*(int*)cfg[index].ptr >> j) & 0b1) +'0';} strTmpBuffer[ind]='\0';
            print_stderr("DEBUG: %s=%s(%d) (file:%s)(int array:%d)\n", var, strTmpBuffer, *(int*)cfg[index].ptr, value, type);
        }
    } else {
        if(debug){print_stderr("DEBUG: invalid type:%d\n", type);}
        return false;
    }

    return true;
}

void config_parse (cfg_vars_t* cfg, unsigned int cfg_size, char* filename, int uid, int gid){ //parse/create program config file
    FILE *filehandle = fopen(filename, "r");
    if (filehandle != NULL) {
        char strBuffer [4096], strTmpBuffer [4096]; //string buffer
        char *tmpPtr, *tmpPtr1, *pos; //pointers
        int cfg_ver=0, line=0;
        while (fgets (strBuffer, 4095, filehandle) != NULL) { //line loop
            line++; //current file line

            //clean line from utf8 bom and whitespaces
            tmpPtr = strBuffer; tmpPtr1 = strTmpBuffer; pos = tmpPtr; //pointers
            if (strstr (strBuffer,"\xEF\xBB\xBF") != NULL) {tmpPtr += 3;} //remove utf8 bom, overkill security?
            while (*tmpPtr != '\0') { //read all chars, copy if not whitespace
                if (tmpPtr - pos > 0) {if(*tmpPtr=='/' && *(tmpPtr-1)=='/'){tmpPtr1--; break;}} //break if // comment
                if(!isspace(*tmpPtr)) {*tmpPtr1++ = *tmpPtr++; //normal char
                }else{tmpPtr++;} //whitespace
            }
            *tmpPtr1='\0'; strcpy(strBuffer, strTmpBuffer); //copy cleaned line

            tmpPtr = strtok (strBuffer, ";"); //split element
            while (tmpPtr != NULL) { //var=val loop
                char strElementBuffer [strlen(tmpPtr)+1]; strcpy (strElementBuffer, tmpPtr); //copy element to new buffer to avoid pointer mess
                tmpPtr1 = strchr(strElementBuffer, '='); //'=' char position
                if (tmpPtr1 != NULL) { //contain '='
                    *tmpPtr1='\0'; int tmpVarSize = strlen(strElementBuffer), tmpValSize = strlen(tmpPtr1+1); //var and val sizes
                    char tmpVar [tmpVarSize+1]; strcpy (tmpVar, strElementBuffer); //extract var
                    char tmpVal [tmpValSize+1]; strcpy (tmpVal, tmpPtr1 + 1); //extract val
                    int tmpIndex = config_search_name (cfg, cfg_size, tmpVar, true); //var in config array
                    if (tmpIndex != -1) { //found in config array
                        config_type_parse (cfg, cfg_size, tmpIndex, cfg[tmpIndex].type, tmpVar, tmpVal);
                    } else if (strcmp (tmpVar, "cfg_version") == 0) { //config version
                        sscanf(tmpVal, "0x%X", &cfg_ver);
                        if(debug){print_stderr("DEBUG: cfg_version=0x%X (%d)\n", cfg_ver, cfg_ver);}
                    } else if (debug) {print_stderr("DEBUG: var '%s'(line:%d) not allowed, typo?\n", tmpVar, line);} //invalid var
                }
                tmpPtr = strtok (NULL, ";"); //next element
            }
        }
        fclose(filehandle);

        //pseudo checksum for config build
        int cfg_ver_org = config_sum (cfg, cfg_size); 
        if(cfg_ver != cfg_ver_org) {
            print_stderr("config file version mismatch (got:%d, should be:%d), forcing save to implement new vars set\n", cfg_ver, cfg_ver_org);
            config_save (cfg, cfg_size, filename, uid, gid, false);
        }
    } else {
        print_stderr("config file not found, creating a new one\n");
        config_save (cfg, cfg_size, filename, uid, gid, false);
    }
}

void config_list (cfg_vars_t* cfg, unsigned int cfg_size){ //print all config vars
    char *rowPtr;
    fprintf(stderr, "Valid config vars:\n");
    for (unsigned int i = 0; i < cfg_size; i++) {
        char tmpVar [strlen(cfg[i].name)+1];
        strcpy (tmpVar, cfg[i].name);
        if (tmpVar[0]=='\n') {rowPtr = tmpVar + 1;} else {rowPtr = tmpVar;}
        fprintf(stderr, "\t'%s': %s\n", rowPtr, cfg[i].desc);
    }
}


