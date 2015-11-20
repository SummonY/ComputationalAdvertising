#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc/malloc.h>
#include <sys/types.h>
#include <locale.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILE_MAX_NUM        1000
#define FILE_MAX_LEN        64
#define BUF_MAX_LEN         1024
#define TABLE_SIZE          200000
#define ABSPATH_MAX_LEN     128
#define FILENAME_MAX_LEN    64
#define ITEM_MAX_NUM        32
#define WORD_MAX_NUM        20000
#define WORD_MAX_LEN        64



unsigned long cryptTable[0x500];
int HashATable[TABLE_SIZE];
int HashBTable[TABLE_SIZE];


struct table {
    char bExists;
};

struct table HASHTABLE[TABLE_SIZE] = {0};


typedef struct doc_node {
    char id[WORD_MAX_LEN];
    int classOne;
    char classTwo[WORD_MAX_LEN];
    int classThree;
    char time[WORD_MAX_LEN];
    char md5[WORD_MAX_LEN];
    int weight;
    struct doc_node *next;
} DOC_NODE, *DOC_LIST;


// 创建关键字结点
typedef struct key_node {
    char *pkey;             // 关键词实体
    int count;              // 关键词出现个数
    int pos;                // 关键词在hash表中的位置
    struct doc_node *next;   // 指向文档结点
} KEY_NODE, *KEY_LISE;

KEY_LISE key_array[TABLE_SIZE] = {0};

// 创建Hash Value 结构体
typedef struct hash_value {
    unsigned int nHash;
    unsigned int nHashA;
    unsigned int nHashB;
    unsigned int nHashStart;
    unsigned int nHashPos;
} HASHVALUE;


static char filename[FILE_MAX_NUM][FILE_MAX_LEN];       // 记录文件
static char words[WORD_MAX_NUM][WORD_MAX_LEN] = {0};    // 记录字个数
static char items[ITEM_MAX_NUM][WORD_MAX_LEN] = {0};    // 记录元素项个数



/* 
 * 得到目录下所有文件名
 */
int GetFileName(char filename[][FILE_MAX_LEN]) {
    int fileNum = 0;
    char *dir = "./12";

    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;
    
    if ((dp = opendir(dir)) == NULL) {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return -1;
    }

    chdir(dir);
    while ((entry = readdir(dp)) != NULL) {
        if (S_ISDIR(statbuf.st_mode)) {
            printf("IS DIR");
        } else {
            printf("%s\n", entry->d_name);
            strcpy(filename[fileNum++], entry->d_name);
        }
    }
    closedir(dp);
    
    return fileNum;
}


/*
 * 将索引信息写入文档
 */
void WriteFile(KEY_LISE keyList, int num, FILE *fwp, int *count) {
    DOC_LIST docList = keyList->next;
    for (int i = 0; i <= num; ++i) {
        fprintf(fwp, "%s %d %s %d %s %d %s %d#####", docList->time, count[i], docList->id, docList->classOne, docList->classTwo, docList->classThree, docList->md5, docList->weight);
        docList = docList->next;
        for (int k = 0; k < count[i] - 1; ++k) {
            fprintf(fwp, "%s %d %s %d %s %d#####", docList->id, docList->classOne, docList->classTwo, docList->classThree, docList->md5, docList->weight);
            docList = docList->next;
        }
        fprintf(fwp, "\n", NULL);
    }
}


/*
 * 以读方式打开文件，如果成功返回文件指针
 */
FILE *OpenReadFile(int index, char filename[][FILE_MAX_LEN]) {
    char *abspath = (char *)malloc(ABSPATH_MAX_LEN);
    char *dir = "./12";

    chdir(dir);
    strncat(abspath, filename[index], FILENAME_MAX_LEN);
    FILE *fp = fopen(abspath, "r");
    if (fp == NULL) {
        printf("Open read file error!\n");
        return NULL;
    } else {
        return fp;
    }
}


/*
 * 以写方式打开文件，如果成功返回文件指针
 */
FILE *OpenWriteFile(const char *filename) {
    if (filename == NULL) {
        printf("output file path error!");
        return NULL;
    }
    FILE *fp = fopen(filename, "w+");
    if (fp == NULL) {
        printf("Open write file error!\n");
    }
    
    return fp;
}

/*
 * 初始化加密表
 */
void PrepareCryptTable() {
    unsigned long seed = 0x00100001, index1 = 0, index2 = 0, i;
    unsigned long tmp1, tmp2;

    for (index1 = 0; index1 < 0x100; ++index1) {
        for (index2 = index1, i = 0; i < 5; ++i, index2 += 0x100) {
            seed = (seed * 125 + 3) % 0x2AAAAB;
            tmp1 = (seed & 0XFFFF) << 0x10;
            seed = (seed * 125 + 3) % 0x2AAAAB;
            tmp2 = (seed & 0XFFFF);
            cryptTable[index2] = (tmp1 | tmp2);
        }
    }
}


/*
 * 获取Hash字符串
 */
unsigned long HashString(const char *keyName, unsigned long hashType) {
    unsigned char *key = (unsigned char *)keyName;
    unsigned long seed1 = 0x7FED7FED;
    unsigned long seed2 = 0xEEEEEEEE;
    int ch;

    while (*key != 0) {
        ch = *key++;
        seed1 = cryptTable[(hashType << 8) + ch] ^ (seed1 + seed2);
        seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
    }
    return seed1;
}


/*
 * 处理字符串末尾空白字符和空白行
 */
int GetRealString(char *pbuf) {
    int len = strlen(pbuf) - 1;
    while (len > 0 && (pbuf[len] == (char)0x0d || pbuf[len] == (char)0x0a || pbuf[len] == ' ' || pbuf[len] == '\t')) {
        --len;
    }

    if (len < 0) {
        *pbuf = '\0';
        return len;
    }
    pbuf[len + 1] = '\0';
    return len + 1;
}


/*
 * 从行缓冲区中得到各项信息，将其写入item数组
 */
void GetItems(char *&move, int &count, int &wordNum) {
    char *front = move;
    bool flag = false;
    int len;

    move = strstr(move, "#####");
    if (*(move + 5) == '#') {
        flag = true;
    }
    if (move) {
        len = move - front;
        strncpy(items[count], front, len);  // 从front 拷贝len个字符到items[count]
    }
    items[count][len] = '\0';
    ++count;
    if (flag) {
        move = move + 10;
    } else {
        move = move + 5;
    }
}

/*
 * 保存关键字相应的文档内容
 */
DOC_LIST SaveItems() {
    DOC_LIST docList = (DOC_LIST)malloc(sizeof(DOC_NODE));
    strcpy(docList->id, items[0]);
    docList->classOne = atoi(items[1]);
    strcpy(docList->classTwo, items[2]);
    docList->classThree = atoi(items[3]);
    strcpy(docList->time, items[4]);
    strcpy(docList->md5, items[5]);
    docList->weight = atoi(items[6]);
    return docList;
}

/*
 * 初始化字符串str的HashValue
 */
void InitHashValue(const char *str, HASHVALUE *hashValue) {
    const int HASH_OFFSET = 0, HASH_A = 1, HASH_B = 2;
    hashValue->nHash = HashString(str, HASH_OFFSET);
    hashValue->nHashA = HashString(str, HASH_A);
    hashValue->nHashB = HashString(str, HASH_B);
    hashValue->nHashStart = hashValue->nHash % TABLE_SIZE;
    hashValue->nHashPos = hashValue->nHashStart;
}


/*
 * 按关键字查询，如果成功返回hash表的索引位置
 */
KEY_LISE SearchByString(const char *src, HASHVALUE *hashValue) {
    unsigned int nHash = hashValue->nHash;
    unsigned int nHashA = hashValue->nHashA;
    unsigned int nHashB = hashValue->nHashB;
    unsigned int nHashStart = hashValue->nHashStart;
    unsigned int nHashPos = hashValue->nHashPos;

    while (HASHTABLE[nHashPos].bExists) {
        if (HashATable[nHashPos] == nHashA && HashBTable[nHashPos] == nHashB) {
            break;
        } else {
            nHashPos = (nHashPos + 1) % TABLE_SIZE;
        }

        if (nHashPos == nHashStart) {
            break;
        }
    }

    if (key_array[nHashPos] && strlen(key_array[nHashPos]->pkey)) {
        return key_array[nHashPos];
    }
    return NULL;
}

/*
 * 插入关键字，如果成功返回hash值
 */
int InsertString(const char *src, HASHVALUE *hashValue) {
    unsigned int nHash = hashValue->nHash;
    unsigned int nHashA = hashValue->nHashA;
    unsigned int nHashB = hashValue->nHashB;
    unsigned int nHashStart = hashValue->nHashStart;
    unsigned int nHashPos = hashValue->nHashPos;

    while (HASHTABLE[nHashPos].bExists) {
        nHashPos = (nHashPos + 1) % TABLE_SIZE;
        if (nHashPos == nHashStart) {
            break;
        }
    }

    int len = strlen(src);
    if (!HASHTABLE[nHashPos].bExists && (len < WORD_MAX_LEN)) {
        HashATable[nHashPos] = nHashA;
        HashBTable[nHashPos] = nHashB;
        key_array[nHashPos] = (KEY_NODE *)malloc(sizeof(KEY_NODE) * 1);
        if (key_array[nHashPos] == NULL) {
            printf("10000 EMS ERROR !!!\n");
            return 0;
        }

        if ((key_array[nHashPos]->pkey = (char *)malloc(len + 1)) == NULL) {
            printf("10000 EMS ERROR !!!\n");
            return 0;
        }
        
        memset(key_array[nHashPos]->pkey, 0, len + 1);
        strncpy(key_array[nHashPos]->pkey, src, len);
        *((key_array[nHashPos]->pkey) + len) = 0;
        key_array[nHashPos]->pos = nHashPos;
        key_array[nHashPos]->count = 1;
        key_array[nHashPos]->next = NULL;
        HASHTABLE[nHashPos].bExists = 1;
        return nHashPos;
    }

    if (HASHTABLE[nHashPos].bExists) {
        printf("30000 in the hash table %s !!!\n", src);
    } else {
        printf("90000 strkey error !!!\n");
    }
    return -1;
}


/*
 * 重新修改strcoll字符串比较函数
 */
int strcoll(const void *s1, const void *s2) {
    char *c_s1 = (char *)s1;
    char *c_s2 = (char *)s2;
    while (*c_s1 == *c_s2++) {
        if (*c_s1 == '\0') {
            return 0;
        }
    }
    return *c_s1 - *--c_s2;
}


/*
 * 交换两个邻接点的内容
 */
void SwapDocNode(DOC_LIST p) {
    DOC_LIST q = (DOC_LIST)malloc(sizeof(DOC_NODE));
    strcpy(q->id, p->id);
    q->classOne = p->classOne;
    strcpy(q->classTwo, p->classTwo);
    q->classThree = p->classThree;
    strcpy(q->time, p->time);
    strcpy(q->md5, p->md5);
    q->weight = p->weight;

    strcpy(p->id, p->next->id);
    p->classOne = p->next->classOne;
    strcpy(p->classTwo, p->next->classTwo);
    p->classThree = p->next->classThree;
    strcpy(p->time, p->next->time);
    strcpy(p->md5, p->next->md5);
    p->weight = p->next->weight;

    strcpy(p->next->id, q->id);
    p->next->classOne = q->classOne;
    strcpy(p->next->classTwo, q->classTwo);
    p->next->classThree = q->classThree;
    strcpy(p->next->time, q->time);
    strcpy(p->next->md5, q->md5);
    p->next->weight = q->weight;
}


/*
 * 对链表进行排序, 待优化
 */
void ListSort(KEY_LISE keyList) {
    DOC_LIST p = keyList->next;
    DOC_LIST final = NULL;
    while (true) {
        bool isFinish = true;
        while (p->next != final) {
            if (strcmp(p->time, p->next->time) < 0) {
                SwapDocNode(p);
                isFinish = false;
            }
            p = p->next;
        }
        final = p;
        p = keyList->next;
        if (isFinish || p->next == final) {
            break;
        }
    }
}



/*
 * 主函数
 */
int main()
{
    KEY_LISE keyList;
    char *pbuf, *move;
    int fileNum = GetFileName(filename);
    FILE *frp;
    
    pbuf = (char *) malloc(sizeof(char) * BUF_MAX_LEN);

    memset(pbuf, 0, BUF_MAX_LEN);
    HASHVALUE *hashValue = (HASHVALUE *)malloc(sizeof(HASHVALUE));
    
    FILE *fwp = OpenWriteFile("index.txt");
    if (fwp == NULL) {
        return 0;
    }
    
    PrepareCryptTable();
    
    int wordNum = 0;
    for (int i = 0; i < fileNum; ++i) {
        frp = OpenReadFile(i, filename);
        if (frp == NULL) {
            system("pause");
            return 0;
        }

        // 每次读取一行处理
        while (fgets(pbuf, BUF_MAX_LEN, frp)) {
            int count = 0;
            move = pbuf;
            if (GetRealString(pbuf) <= 1) {
                continue;
            }

            while (move != NULL) {
                while (*move == '#') {
                    move++;
                }

                if (!strcmp(move, "")) {        // 相等返回0，大于返回1，小于返回-1
                    break;
                }

                GetItems(move, count, wordNum);
            }

            for (int i = 7; i < count; ++i) {
                // 将关键字对应的文档加入文档结点链表中
                // 如果关键字第一次出现，则将其加入hash表
                InitHashValue(items[i], hashValue);
                if (keyList = SearchByString(items[i], hashValue)) {
                    DOC_LIST docList = SaveItems();
                    docList->next = keyList->next;
                    keyList->count++;
                    keyList->next = docList;
                } else {
                    int pos = InsertString(items[i], hashValue);
                    keyList = key_array[pos];
                    DOC_LIST docList = SaveItems();
                    docList->next = NULL;
                    keyList->next = docList;
                    if (pos != -1) {
                        strcpy(words[wordNum++], items[i]);
                    }
                }
            }
        }
    }
    
    // 通过快排对关键词进行排序
    qsort(words, WORD_MAX_NUM, WORD_MAX_LEN, strcoll);

    // 遍历关键字数组，将关键字及其对应的文档内容写入文件中
    int rowNum = 1, oneDateFileNum = 0;
    for (int i = 0; i < WORD_MAX_NUM; ++i) {
        InitHashValue(words[i], hashValue);
        keyList = SearchByString(words[i], hashValue);
        if (keyList != NULL) {
            DOC_LIST docList = keyList->next;

            char date[9];
            // 截取年月日
            for (int j = 0; j < keyList->count; ++j) {
                strncpy(date, docList->time, 8);
                date[8] = '\0';
                strncpy(docList->time, date, 9);
                docList = docList->next;
            }

            // 对链表根据时间进行排序
            ListSort(keyList);
            
            docList = keyList->next;
            int *count = (int *)malloc(sizeof(int) * WORD_MAX_NUM);
            memset(count, 0, WORD_MAX_NUM);
            strcpy(date, docList->time);
            oneDateFileNum = 0;
            // 得到单个日期的文档数目
            for (int j = 0; j < keyList->count; ++j) {
                if (strcmp(date, docList->time) == 0) {
                    count[oneDateFileNum]++;
                } else {
                    count[++oneDateFileNum]++;
                }
                strcpy(date, docList->time);
                docList = docList->next;
            }
            fprintf(fwp, "%s %d %d\n", words[i], oneDateFileNum + 1, rowNum);
            WriteFile(keyList, oneDateFileNum, fwp, count);
            rowNum++;
        }
    }
    free(pbuf);
    free(hashValue);
    fclose(frp);
    fclose(fwp);
    return 0;
}
