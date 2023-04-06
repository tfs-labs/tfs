#ifndef TFS_MAIN_H
#define TFS_MAIN_H

 
void menu();
bool init();
bool InitConfig();
bool InitLog();
bool InitAccount();
bool InitRocksDb();

/*********Check Consistency*********/

bool Check();

#endif
