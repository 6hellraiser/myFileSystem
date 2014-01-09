#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

const int cluster_num = 250;
const int bytes_num = 1024 * 250;//cluster_num;
const int N = 50;


typedef struct file {
    int isdir;
    int first_cluster;
    int last_cluster;
    long size;
    char* name;
    //mode
} file;

file* fs;
file* pointers;
int first_free_cluster;
int first_free_file;
//cluster cont[50];
char bytes[1024*250];//[bytes_num];

file* get_file(const char *path)
{   

    printf("getfile\n");    

    int i = 0;

    printf("path --- %s\n",path);
    printf("%s\n",fs[i].name);
    while ((strcmp(path,fs[i].name))!=0)// && i < (sizeof(fs)/sizeof(*fs))))
    {
        printf("ommm\n");
           if (i < first_free_file - 1)
                i++;
           else return NULL;
    }
    //check!!
    printf("got file\n");
    return fs + i;
}

int h_getattr (const char *path, struct stat *stbuf)
{
    /*

Get file attributes.

Similar to stat(). The 'st_dev' and 'st_blksize' fields are ignored. The 'st_ino' field is ignored except if the
 'use_ino' mount option is given.
 ищем файл по path
 вытаскиваем его характеристики
 вернуть настоящий размер файла
*/
    printf("getattr\n");
    printf("path --- %s\n",path);
    int result = 0;
    //if error return error
    file* f = get_file(path);
    if (f != NULL)
    {
        printf("i'm not null");
        stbuf->st_size = f->size;
        if (f->isdir)
        {
            stbuf->st_mode = S_IFDIR;
        }
        else
        {
            stbuf->st_mode = S_IFREG;
        }
    }
    else result = -1;
    return result;
}

int h_mkdir(const char *path, mode_t mode)
{
    /*

Create a directory

Note that the mode argument may not have the type specification bits set, i.e. S_ISDIR(mode) can be false. To obtain
 the correct directory type bits use mode|S_IFDIR
 создаем файл по указанному пути 
 говорим, что isdir = 1
*/
    //file* newdir = (file*)malloc(sizeof(file));
    printf("mkdir\n");
    fs[first_free_file].isdir = 1;
    fs[first_free_file].name = path;
    first_free_file++;
}


void defragment()
{
    /*
unsigned int  array[512];                 // источник
unsigned char byte_array[sizeof(array)];  // буфер назначения

memcpy(byte_array, array, sizeof(array));*/
    printf("defragment\n");
    int flag = 0;
    for (int i = 0; i < bytes_num; i+=4000)
    {
        if (bytes[i] != NULL)
        {
            if (flag)
            {
                if (bytes[i-1] == NULL)
                {
                    pointers[i].first_cluster-=flag;
                    pointers[i].last_cluster-=flag;
                }
                char* src = bytes[i];
                char* dest = bytes[i-flag];
                memcpy(dest,src,sizeof(char)*4000);
                //clean up src
                bytes[i] = NULL;
                pointers[i-flag] = pointers[i];
                memset(pointers+i,'\0',sizeof(file));
                //*(pointers+i) = NULL;
            }
        }
        else
        {
            flag++;
        }
    }
    for (int i = 0; i < bytes_num; i+=4000)
    {
        if (bytes[i] == NULL)
        {
            first_free_cluster = i/4000;
            break;
        }
    }
}

int h_rmdir(const char *path)
{
    /*Remove a directory
		ищем папку по указанному пути

		дефрагментация
    */
    printf("rmdir\n");
    int cnt = sizeof(path)/sizeof(char);
    char* newstr;
    for (int i = 0; i < N; i++)
    {
        if ((fs+i)->name != NULL)
        {
            newstr = strncpy(newstr,(fs+i)->name,cnt);
            if (!strcmp(newstr,path))
            {
                //if dir then delete from fs
                //else delete from bytes then delete from fs
                if (!fs[i].isdir)
                {
                    int start = fs[i].first_cluster*4000;
                    for (int j = start; j < start + fs[i].size; j++)
                    {
                        bytes[j] = NULL;
                    }
                    for (int k = fs[i].first_cluster; k <= fs[i].last_cluster; k++)
                    {
                        //free(pointers[k]);
                        memset(pointers+k,'\0',sizeof(file));
                    }
                }
                memset(fs+i,'\0',sizeof(file));
                //free(fs[i]);
                //fs[i] = NULL;
            }
        }
    }
    defragment();
}


int h_rename(const char *path, const char *newpath)
{
    /*Rename a file
		ищем файл по path
		меняем его атрибут name
	*/
    printf("rename\n");
    file* f = get_file(path);
    f->name = newpath;
}


int h_open(const char *path, struct fuse_file_info *fi)
{
    /*

File open operation

No creation (O_CREAT, O_EXCL) and by default also no truncation (O_TRUNC) flags will be passed to open(). If an application specifies O_TRUNC,
 fuse first calls truncate() and then open(). Only if 'atomic_o_trunc' has been specified and kernel version is 2.6.24 or later, O_TRUNC is passed 
 on to open.
Unless the 'default_permissions' mount option is given, open should check if the operation is permitted for the given flags. Optionally open 
may also return an arbitrary filehandle in the fuse_file_info structure, which will be passed to all file operations.

Changed in version 2.2
	проверка доступа к файлу
*/
    printf("open\n");
    int i = 0;    

    for (i = 0; i < first_free_file; i++)
    {
        if ((strcmp(path,fs[i].name) == 0) && (fs[i].isdir == 0))
            return 0;
    }
    return -ENOENT;
}

int h_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    /*

Read data from an open file

Read should return exactly the number of bytes requested except on EOF or error, otherwise the rest of the data will be substituted with 
zeroes. An exception to this is when the 'direct_io' mount option is specified, in which case the return value of the read system call will
 reflect the return value of this operation.

Changed in version 2.2
	изучаем fi!!!
	ищем файл по path
	ищем первый кластер с файлом
	делаем offset
	for (i = offset; i < offset + size; i++)
		читаем данные в buf
		хэ = считаем прочитанные байты
	возвращаем хэ
*/
    printf("read\n");
   // fflush(stdout);
   // printf("%s\n",path);
    if (path != NULL) printf("not null\n"); else printf("null\n");
    file* f = get_file(path);

    printf("strange\n");
    for (int i = f->first_cluster*4000; i <= f->last_cluster*4000; i++)
    {
        memcpy(buf[i],bytes[i],sizeof(char));
    }
    return size;
}

int h_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    /*

Write data to an open file

Write should return exactly the number of bytes requested except on error. An exception to this is when the 'direct_io' mount option is specified
 (see read operation).

Changed in version 2.2
	дилемма: наверно, и здесь нужна дефрагментация, если объем записываемых данных больше размера файла
	а так аналогично чтению
*/
    printf("write\n");
    file* g = get_file(path);
    int flag = 0;
    if (g->size > 0)
    {
        flag = 1;
        fs[first_free_file].isdir = g->isdir;
        fs[first_free_file].name = g->name;
        fs[first_free_file].size = 0;
        first_free_file++;
        //f = NULL;
        for (int i = g->first_cluster; i <= g->last_cluster; i++)
        {
           // pointers[i] = NULL;
            memset(pointers+i,'\0',sizeof(file));
            bytes[i*4000] = NULL;
        }
        free(g);
        //g = NULL;
    }

    file* f = get_file(path);
    //if f.size == 0 -> get place in bytes
    f->size += size;

    f->first_cluster = first_free_cluster;
    int clust = sizeof(char)*4000;
    size -= clust;
    float check = size/clust;
    int cnt_clusters = size/clust;
    if ((check - cnt_clusters)>0)
    {
        cnt_clusters += 1;
    }
    f->last_cluster = f->first_cluster + cnt_clusters;
    /*for (int i = first_free_cluster; i < first_free_cluster + cnt_clusters; i++)
    {
        pointers[i] = f;
    }*/
    for (int j = f->first_cluster*4000/* + offset*/; j <= f->last_cluster*4000; j++)
    {
        memcpy(bytes[j],buf[j],sizeof(char));
    }

    //change first free cluster
    first_free_cluster = f->last_cluster + 1;
    if (flag)
        defragment();
    return size;
}

int h_opendir(const char *path, struct fuse_file_info *fi)
{

    printf("opendir\n");
    int i = 0;

    int flag = 0;
    while(i < first_free_file)
    {
        printf("%s\n",path);
        printf("%s\n",fs[i].name);
        if (strcmp(path,fs[i].name) != 0)
            i++;
        else
        {
            flag = 1;
            break;
        }
        printf("end\n");
    }
    printf("%d\n",flag);
    if (flag == 0)
        return -ENOENT;
    else return 0;
}

int h_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
	/*Read directory

The filesystem may choose between two modes of operation:

1) The readdir implementation ignores the offset parameter, and passes zero to the filler function's offset. The filler function will not
 return '1' (unless an error happens), so the whole directory is read in a single readdir operation.

2) The readdir implementation keeps track of the offsets of the directory entries. It uses the offset parameter and always passes non-zero 
offset to the filler function. When the buffer is full (or an error happens) the filler function will return '1'.

Introduced in version 2.3

 * This supersedes the old getdir() interface.  New applications
 * should use this.
???
*/
    printf("readdir\n");
    file* f = get_file(path);
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    printf("readingdir1\n");

    int cnt = sizeof(path)/sizeof(char);
    char* newstr = (char*)malloc(sizeof(char));
    for (int i = 0; i < first_free_file; i++)
    {
        printf("readdir2\n");
        newstr = strncpy(newstr,(fs+i)->name,cnt);        
        printf("%s\n",newstr);
        printf("path-- %s\n",path);
        if ((!strcmp(newstr,path)) && (strcmp((fs+i)->name,path)))
        {
            printf("entered\n");
            filler(buf,*((fs+i)->name + cnt),NULL,0);
            printf("filled\n");
        }
        printf("iteration\n");
    }
}		   


void *h_init(struct fuse_conn_info *conn)
{

/*
    Initialize filesystem

    The return value will passed in the private_data field of fuse_context to all file operations and as a parameter to
the destroy() method.

    Introduced in version 2.3 Changed in version 2.6
	???
    */
    printf("init\n");
    fs = (file*)malloc(sizeof(file)*N);
    pointers = (file*)malloc(sizeof(file)*cluster_num);
    first_free_cluster = 0;
    first_free_file = 0;
    fs[0].isdir = 1;
    fs[0].name = "/";
    printf("%s\n",fs[0].name);
    fs[0].size = 0;
    first_free_file++;
}

void h_destroy(void *userdata)
{
    /*

Clean up filesystem

Called on filesystem exit.

Introduced in version 2.3
*/
    printf("getfile\n");
    free(fs);
    free(pointers);
    int i = 0;
    while(bytes[i] != NULL)
    {
        bytes[i] = NULL;
        i++;
    }
}

int h_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    /*


Create and open a file

If the file does not exist, first create it with the specified mode, and then open it.

If this method is not implemented or under Linux kernel versions earlier than 2.6.15, the mknod() and open() methods will be called instead.

Introduced in version 2.5
	создаем новую структуру
	пишем все атрибуты
	помещаем файл в кластер(ы)
*/
   // file* newfile = (file*)malloc(sizeof(file));
    printf("create\n");
    fs[first_free_file].isdir = 0;
    fs[first_free_file].name = path;
    fs[first_free_file].size = 0;
    first_free_file++;
}

static struct fuse_operations oper = {   
    .open       = h_open    ,
    .read       = h_read      ,
    .write      = h_write     ,
    .opendir    = h_opendir   ,
    .readdir    = h_readdir   ,
    .init       = h_init      ,
    .create     = h_create    ,
    .rename = h_rename,
    .mkdir = h_mkdir,
    .getattr = h_getattr,
    .rmdir = h_rmdir,
};

int main(int argc, char *argv[])
{

    return fuse_main(argc, argv, &oper, NULL);
}

