#include<stdio.h>
#include<string.h>
#include"fstemplate.h"

#define MAXFNAME	14
#define BLKSIZE		512

#define INODE_BLK_STARTS_AT 3
#define DATA_BLK_STARTS_AT 11
#define SUP_BLK_STARTS_AT 1
#define EMPTY 0

// Data structure definitions

struct  SupBlock;
struct INode;

int MkFS(int dev, int ninodes, int nrootdir, int blksize);
int my_memcpy (const void* dest,const void* src, int size);
int OpenFile(int dirhandle, char *fname, int mode, int uid, int gid, int attrib);
int CloseFile(int fhandle);
int CloseFile(int fhandle);
int WriteFile(int fhandle, char buf[], int nbytes);
int ReadInode(int dev, int ino, struct INode *inode);
int WriteInode(int dev, int ino, struct INode *inode);
int readSuper (int dev, struct SupBlock* sb);
int writeSuper (int dev,struct SupBlock* sb);
int AllocInode(int dev);
int FreeInode(int dev, int ino);
int AllocBlk(int dev);
int FreeBlk(int dev, int blk);
int ReadBlock(int dev, int blk, char buf[BLKSIZE]);
int WriteBlock(int dev, int blk,char buf[BLKSIZE]);
int OpenDevice(int dev);
int ShutdownDevice(int dev);
/*
int init_fs(void);
void cleanup_fs(void);
void cleanup_module (void);
int init_module (void);


void read_inode (struct inode*  _inode);
void destroy_inode (struct inode* _inode);
struct inode* alloc_inode (struct super_block* sb);
void write_inode (struct inode* _inode, int unknown);
void write_super (struct super_block*  kernel_sb );
struct super_block* read_super(struct super_block* sb, void * data, int silent);
*/





struct SupBlock {
	char sb_vname[MAXFNAME];
	int	sb_nino;
	int	sb_nblk;
	int	sb_nrootdir;
	int	sb_nfreeblk;
	int	sb_nfreeino;
	int	sb_flags;
	unsigned short sb_freeblks[BLKSIZE/sizeof(unsigned short)];
	int	sb_freeblkindex;
	int	sb_freeinoindex;
	unsigned int	sb_chktime;
	unsigned int	sb_ctime;
	unsigned short sb_freeinos[(BLKSIZE - (54))/2];
};



struct OnDiskDirEntry {
	char	d_name[MAXFNAME];
	unsigned short	d_ino;
};





struct DirEntry {
	struct OnDiskDirEntry d_entry;
	int	d_offset;
} *dir_table[10];





#define ROOTDIRSIZE	((4 * 512)/sizeof(struct OnDiskDirEntry))

struct INode {
	unsigned int	i_size;
	unsigned int	i_atime;
	unsigned int	i_ctime;
	unsigned int	i_mtime;
	unsigned short	i_blks[13];
	short		i_mode;
	unsigned char	i_uid;
	unsigned char	i_gid;
	unsigned char	i_gen;
	unsigned char	i_lnk;
};

#define INODETABSIZE	(( 512 / sizeof(struct INode))*8)

struct InCoreINode {
	struct INode 	ic_inode;
	int 		ic_ref;
	int		ic_ino;
	short		ic_dev;
	struct InCoreINode	*ic_next;
	struct InCoreINode	*ic_prev;
} *head;


struct OpenFileObject {
	struct InCoreINode	*ofo_inode;
	int			ofo_mode;
	unsigned int		ofo_curpos;
	int			ofo_ref;
}*ofo_table[10];



//============= TESTING APPLICATION USING THESE FS CALLS ==============

// Menu driven testing application for creation of fs,
// and all file and directory related operations

char *devfiles[] = {"TEST",NULL};
int devfd[] = {-1, -1};
int dev =0;

int main()
{

    devfd[0]=open("new_disk",0666);
    MkFS(dev,INODETABSIZE,ROOTDIRSIZE,8192);

    // initially all files and directories are closed
    int i;
    for (i=0;i<10;i++) {
        ofo_table[i]=NULL;
        dir_table[i]=NULL;
    }
    //check functionality here

    return 0;

}



//============= SYSTEM CALL LEVEL NOT FOLLOWED =======


//============= VNODE/VFS NOT FOLLOWED ===============

//============== UFS INTERFACE LAYER ==========================


int MkFS(int dev, int ninodes, int nrootdir, int blksize)
{
	int rootentrycount = nrootdir; //ROOTDIRSIZE;
	int inodecount = ninodes;// INODETABSIZE;
	int blkcount = blksize;//8192;
	char buf[BLKSIZE];
	struct SupBlock sb;
	int reservblks = 1 + 2 + 8 + 4; // boot, super, inodetable, rootdir blks

	// Assuming only predefined sizes are used
	// Otherwise we have to recompute according to the parameters given.
	//

	// Boot block dummy block (Because no boot loader nothing...)
	//bzero(buf, BLKSIZE);
	WriteBlock(devfd[dev],0,buf);
	//write(devfd[dev], buf, 512);

	// Write initialized superblock
	strcpy(sb.sb_vname, "TESTFILESYS");
	sb.sb_nino = inodecount;
	sb.sb_nblk = blkcount;
	sb.sb_nrootdir = rootentrycount;
	sb.sb_nfreeblk = blkcount - reservblks;
	sb.sb_nfreeino = inodecount;
	sb.sb_flags = 0;//FS_CLEAN;
	sb.	sb_freeblkindex = (BLKSIZE/sizeof(unsigned short))-1;
	sb.	sb_freeinoindex = (BLKSIZE - (54))/2 - 1;
	sb.sb_chktime = 0;
	sb.sb_ctime = 0;

	int i;
	for (i=0;i<(BLKSIZE-54)/2;i++) {
		sb.sb_freeinos[i]= i+2;//counting of inodes starts from 1 (not 0),root directory inode no.=1
	}

	for (i=1;i<BLKSIZE/sizeof(unsigned short);i++)
		sb.sb_freeblks[i]=i+3;// counting starts from 0 and blkno. 0,1,2,3 are alloted to root directory

    int next_freeblk_list=i+3;
    //assignning pointer to free block containing next free blk list
    sb.sb_freeblks[0]=next_freeblk_list;
	//write(devfd[dev],&sb,sizeof(sb));
	writeSuper (dev,&sb);



	// Write initialized list of inodes

    struct INode* inode_ptr=(struct INode*) buf;

    for (i=0;i<BLKSIZE/sizeof(struct INode);i++) {
        inode_ptr->i_mode=0x0fff;
        inode_ptr++;
    }

    inode_ptr=(struct INode*) buf;

    //alloting first Inode for root directory file
    inode_ptr->i_mode=0x2fff;//mode 4 specifies that it is a directory , 1 as normal file
    inode_ptr->i_size=BLKSIZE*4;
    inode_ptr->i_gid=255;
    inode_ptr->i_uid=254;
    inode_ptr->i_lnk=1; //root directory hardlinks =1
    inode_ptr->i_gen=1;
    inode_ptr->i_blks[0]=0;
    inode_ptr->i_blks[1]=1;
    inode_ptr->i_blks[2]=2;
    inode_ptr->i_blks[3]=3;
    //only 4 blks are allocated

    WriteBlock(devfd[dev],INODE_BLK_STARTS_AT,buf);

    //REST 7 BLKS OF INODE BLOCKS ARE EMPTY
    inode_ptr=(struct INode*) buf;
    inode_ptr->i_mode=0x0fff;
    //write 7 empty inode blocks
    for(i=1;i<=7;i++)
        WriteBlock(dev,INODE_BLK_STARTS_AT+i,buf);


	// Write initialized list of directory entries

    struct OnDiskDirEntry* dirptr=(struct OnDiskDirEntry*)buf;
    dirptr->d_ino=1;
    strcpy(dirptr->d_name,".");
    dirptr++;
    dirptr->d_ino=1;
    strcpy(dirptr->d_name,"..");// root and its parent have inode no 1
    dirptr++;
    for (i=2;i<BLKSIZE/sizeof(struct OnDiskDirEntry);i++) {
        dirptr ->d_ino=0; //rest entries are currently empty
        dirptr++;
    }

    WriteBlock (dev,DATA_BLK_STARTS_AT+0,buf);

	dirptr= (struct OnDiskDirEntry*) buf;
	dirptr->d_ino=0;
	dirptr++;
	dirptr->d_ino=0;

    //write rest 3 directory blocks(empty)
	for (i=1;i<4;i++)
        WriteBlock(dev,DATA_BLK_STARTS_AT+i,buf);


	// Fill the remaining empty datablocks

    //bzero(buf, BLKSIZE);
    unsigned short* ptr=(short*)buf;
    for (i=DATA_BLK_STARTS_AT+4;i<blkcount-reservblks;i++) {

        if (i==next_freeblk_list) {
            int j;
            for (j=1;j<BLKSIZE/sizeof(short);j++) {
                ptr[j]=next_freeblk_list+j;
            }
            next_freeblk_list += BLKSIZE/sizeof(short);
            ptr[0]=next_freeblk_list;
        }

        WriteBlock(dev,i, buf);
    }



	// Write free blk information (data structures)
}



//utility function
int my_memcpy (const void* dest,const void* src, int size) {
    char* d=(char* ) dest;
    char* s=(char* ) src;
    int i;
    for (i=0;i<size;i++) {
        *d=*s;
        d++;s++;
    }
    return i;
}

// Open/create a file in the given directory with specified uid, gid
// and attributes. Type of modes are limited to read, write and append only.



int OpenFile(int dirhandle, char *fname, int mode, int uid, int gid, int attrib)
{
    struct INode inode;
    ReadInode(dev,dir_table[dirhandle]->d_entry.d_ino,&inode);

    int done=0, i=0;
    char buf[BLKSIZE];

    int data_blks_allocated_to_dir= inode.i_size/BLKSIZE + (inode.i_size % BLKSIZE)==0?0:1 ;
    struct OnDiskDirEntry* dp;

    while (done==0 && i<data_blks_allocated_to_dir) // assuming dirctory-file is small(no need to look in indirect,double indirect.)
    {
        int blkno=inode.i_blks[i];
        ReadBlock(dev,DATA_BLK_STARTS_AT+blkno,buf);
        dp= (struct OnDiskDirEntry*) buf;
        int j=0;
        while (done==0 && j<BLKSIZE/sizeof (struct OnDiskDirEntry)) {
            if (strcmp (dp->d_name,fname)==0) {
                done = 1;
                continue;
            }
            j++;dp++;
        }
        i++;
    }
    if (done==1) //if found
    {
        i=0;
        while (ofo_table[i]!=NULL&&i<10)
            i++;

        if (i>=10) {
            printf("can't open more than 10 files\n");
            return -1;
        }

        int file_descriptor= i;

        ofo_table[file_descriptor]= (struct OpenFileObject*) malloc(sizeof(struct OpenFileObject));

        // check if file is already open ??
        struct InCoreINode* iterator = head;
        int found=0;
        while (found==0 && iterator!=NULL) {

            if (iterator->ic_ino==dp->d_ino)  {
                found=1;
                continue;
            }
            iterator = iterator->ic_next;
        }
        if (found==1) // file has already opened by someother process
        {
            //per-process table have pointer to system wide table
            ofo_table[file_descriptor]->ofo_inode=iterator;
        }
        else {
            // file has not opened so add a new entry in system wide open file table
            struct InCoreINode* new_entry;
            new_entry= (struct InCoreINode* ) malloc(sizeof(struct InCoreINode));
            //adding in the front of linklist (system-wide table)
            new_entry->ic_next=head;
            head->ic_prev=new_entry;
            head=new_entry;
            //per-process table have pointer to system wide table
            ofo_table[file_descriptor]->ofo_inode=new_entry;
        }
        // assign rest fields per-process open file table
        ofo_table[file_descriptor]->ofo_curpos=0;
        ofo_table[file_descriptor]->ofo_inode=dp->d_ino;
        ofo_table[file_descriptor]->ofo_mode=mode;

        return file_descriptor;
    }

    //file not found so lets create it, then open it then return its file descriptor
    return CreateFile(dirhandle,fname,mode,uid,gid,attrib);

}



int CreateFile(int dirhandle, char *fname, int mode, int uid, int gid, int attrib)
{

//allocating a inode to the file to be created
    int free_inode= AllocInode(dev);
    struct INode inode;
    ReadInode(dev,free_inode,&inode);

    inode.i_mode=0x1755;// regular file
    inode.i_size=0;// initially file is empty
    inode.i_gen++;// each time when inode is use for new file increase its gen (following unix)
    inode.i_lnk=1;
    inode.i_uid=uid;
    inode.i_gid=gid;

    WriteInode(dev,free_inode,&inode);

    // adding a directory entry into the given directory
    int done=0, i=0;
    char buf[BLKSIZE];

    int data_blks_allocated_to_dir= inode.i_size/BLKSIZE + (inode.i_size % BLKSIZE)==0?0:1 ;

    struct OnDiskDirEntry* dp;

    while (done==0 && i<data_blks_allocated_to_dir )   // assuming size is small(no need to look in indirect,double indirect.)
    {
        int blkno=inode.i_blks[i];
        ReadBlock(dev,DATA_BLK_STARTS_AT+blkno,buf);
        dp= (struct OnDiskDirEntry*) buf;
        int j=0;
        while (done==0 && j<BLKSIZE/sizeof (struct OnDiskDirEntry)) //get a free directory entry(having inode no=0)
        {
            if (dp->d_ino==0) {
                done = 1;
                dp->d_ino=free_inode;
                strcpy(dp->d_name,fname);
                WriteBlock(dev,DATA_BLK_STARTS_AT+blkno,buf);
                continue;
            }
            j++;dp++;
        }
        i++;
    }
    //direcotory has already got an entry of new file

    //new open the file and return its file descriptor

    //add a entry in system wide open file table
    struct InCoreINode* new_entry;
    new_entry= (struct InCoreINode* ) malloc(sizeof(struct InCoreINode));

    // copy disk inode to incore-inode
    my_memcpy(&new_entry->ic_inode,&inode,sizeof (struct INode));

    // initialize other fields
    new_entry->ic_ino=dp->d_ino;
    new_entry->ic_dev=dev;
    new_entry->ic_ref=1; //currently 1 instance of file is open

    //adding in the front of linklist (system-wide table)
    new_entry->ic_next=head;
    head->ic_prev=new_entry;
    head=new_entry;

    //search a free entry in open file table
    i=0;
    while (ofo_table[i]!=NULL)
        i++;

    int file_descriptor =i;

    ofo_table[file_descriptor]= (struct OpenFileObject*) malloc(sizeof(struct OpenFileObject));
    //per process entry points to corresponding entry in system wide table

    ofo_table[file_descriptor]->ofo_inode= new_entry;

    //initialize other additional fields
    ofo_table[file_descriptor]->ofo_curpos=0;
    ofo_table[file_descriptor]->ofo_inode=dp->d_ino;
    ofo_table[file_descriptor]->ofo_mode=mode;
    ofo_table[file_descriptor]->ofo_ref=0;

    return file_descriptor;

}



// Close a file
int CloseFile(int fhandle)
{
    if (ofo_table[fhandle]==NULL) //invalid file descriptor
        return -1;

    //decrease its ref count in system wide table
    ofo_table[fhandle]->ofo_inode->ic_ref--;
    if (ofo_table[fhandle]->ofo_inode->ic_ref ==0) // if ref count is 0 then remove its entry from system wide table
    {
        struct InCoreINode* node_tobe_remove= ofo_table[fhandle]->ofo_inode;
        if(node_tobe_remove->ic_prev!=NULL)
        {
            node_tobe_remove->ic_prev->ic_next=node_tobe_remove->ic_next;
        }
        if (node_tobe_remove->ic_next!=NULL)
        {
            node_tobe_remove->ic_next->ic_prev=node_tobe_remove->ic_prev;
        }
        if (head==node_tobe_remove)
            head = head ->ic_next;

        free(node_tobe_remove);

    }
    //remove entry from per process table
    free(ofo_table[fhandle]);
    ofo_table[fhandle]=NULL;

    return 0;

}



// Read from a file already opened, nbytes into buf
int ReadFile(int fhandle, char buf[], int nbytes)
{
    if (fhandle<0||ofo_table[fhandle]==NULL) {
        return -1;//error
    }

    struct INode inode;
    ReadInode( dev,ofo_table[fhandle]->ofo_inode->ic_ino,&inode);

    int nblks_to_be_read= (nbytes-1)/BLKSIZE +1;
    int nblk_to_be_skipped= (ofo_table[fhandle]->ofo_curpos -1) / BLKSIZE;

    int first_blkno_tobe_read= inode.i_blks[nblk_to_be_skipped];

    int byte_to_be_skipped= (ofo_table[fhandle]->ofo_curpos-1) % BLKSIZE;

    char tempbuf [BLKSIZE];
    ReadBlock(ofo_table[fhandle]->ofo_inode->ic_dev, first_blkno_tobe_read, tempbuf);

    int bytes_tobe_read= BLKSIZE - byte_to_be_skipped;
    my_memcpy(buf,tempbuf+byte_to_be_skipped, bytes_tobe_read );

    char* buf_ptr=buf+bytes_tobe_read;
    //now read remaining blks
    int i=nblk_to_be_skipped +1;
    for (;i<nblks_to_be_read;i++) {

        ReadBlock(dev,DATA_BLK_STARTS_AT+inode.i_blks[i],tempbuf);

        int bytes_to_transfer;

        if (nbytes>(i+1)*BLKSIZE)
            bytes_to_transfer=BLKSIZE;
        else bytes_to_transfer= nbytes-i*BLKSIZE;

        my_memcpy(buf_ptr,tempbuf,bytes_to_transfer);
        buf_ptr += BLKSIZE;
    }
    // advance the cursor
    ofo_table[fhandle]->ofo_curpos +=  nbytes;

    return nbytes; // return bytes read

}



// Write into a file already opened, nbytes from buf
int WriteFile(int fhandle, char buf[], int nbytes)
{

    // implementation of writefile is same as above readfile function, just direction of flow of data is reverse

    if (fhandle<0||ofo_table[fhandle]==NULL) {
        return -1;//error
    }

    struct INode inode;
    ReadInode( dev,ofo_table[fhandle]->ofo_inode->ic_ino,&inode);

    int nblks_to_be_read= (nbytes-1)/BLKSIZE +1;
    int nblk_to_be_skipped= (ofo_table[fhandle]->ofo_curpos -1) / BLKSIZE;

    int first_blkno_tobe_read= inode.i_blks[nblk_to_be_skipped];

    int byte_to_be_skipped= (ofo_table[fhandle]->ofo_curpos-1) % BLKSIZE;

    char tempbuf [BLKSIZE];
    ReadBlock(ofo_table[fhandle]->ofo_inode->ic_dev, first_blkno_tobe_read, tempbuf);

    int bytes_tobe_read= BLKSIZE - byte_to_be_skipped;
    my_memcpy(tempbuf+byte_to_be_skipped, buf, bytes_tobe_read );

    char* buf_ptr=buf+bytes_tobe_read;
    //now read remaining blks
    int i=nblk_to_be_skipped +1;
    for (;i<nblks_to_be_read;i++) {

        ReadBlock(dev,DATA_BLK_STARTS_AT+inode.i_blks[i],tempbuf);

        int bytes_to_transfer;

        if (nbytes>(i+1)*BLKSIZE)
            bytes_to_transfer=BLKSIZE;
        else bytes_to_transfer= nbytes-i*BLKSIZE;

        my_memcpy(tempbuf, buf_ptr, bytes_to_transfer);
        buf_ptr += BLKSIZE;
    }
    // advance the cursor
    ofo_table[fhandle]->ofo_curpos +=  nbytes;

    return nbytes; // return bytes read
}



// Move the file pointer to required position
int SeekFile(int fhandle, int pos, int whence)
{
    if (ofo_table[fhandle]==NULL)
        return -1;

    if (whence==SEEK_SET) {
        ofo_table[fhandle]->ofo_curpos = pos;
    }
    else if (whence==SEEK_CUR) {
        ofo_table[fhandle]->ofo_curpos += pos;
    }
    else if(whence==SEEK_END) {

        struct INode inode;
        ReadInode(dev,ofo_table[fhandle]->ofo_inode->ic_ino, &inode);

        ofo_table[fhandle]->ofo_curpos = inode.i_size+pos;
    }
    return ofo_table[fhandle]->ofo_curpos;
}



// Create a directory
int MkDir(int pardir, char *dname, int uid, int gid, int attrib)
{
    struct OnDiskDirEntry new_dir;
    strcpy (new_dir.d_name,dname);
    int dev=0;
    new_dir.d_ino= AllocInode(devfd[dev]);

    struct INode inode;
    inode.i_gid=gid;
    inode.i_uid=uid;
    inode.i_mode= 0x4000 | attrib;
    inode.i_lnk=1;
    inode.i_size=0;
    inode.i_gen++;
    WriteInode (dev,new_dir.d_ino,&inode);

    struct DirEntry new_dentry;
    my_memcpy(&new_dentry,&new_dir,sizeof(struct OnDiskDirEntry));

    SeekDir(pardir,0,SEEK_END);
    WriteDir(pardir, &new_dentry);
}




// Delete directory (if itis empty)
int RmDir(int pardir, char *dname)
{
    struct DirEntry* parent = dir_table[pardir];
    struct INode inode;
    ReadInode(0,parent->d_entry.d_ino,&inode);

    char buf[BLKSIZE];
    struct OnDiskDirEntry* dp;

    int data_blks_allocated_to_dir= inode.i_size/BLKSIZE + (inode.i_size % BLKSIZE)==0?0:1 ;
    int i=0,done=0;
    int blkno;
    while (done==0&&i<data_blks_allocated_to_dir)  // assuming dirctory-file is small(no need to look in indirect,double indirect.)
    {
        blkno=inode.i_blks[i];
        ReadBlock(dev,DATA_BLK_STARTS_AT+blkno,buf);

        dp= (struct OnDiskDirEntry*) buf;
        int j=0;
        while (done==0 && j<BLKSIZE/sizeof (struct OnDiskDirEntry)) {
            if (strcmp (dp->d_name,dname)==0) {
                done = 1;
                continue;
            }
            j++;dp++;
        }
        i++;
    }

    struct INode inode_of_dir_to_be_remove;
    ReadInode (dev,dp->d_ino,&inode_of_dir_to_be_remove);

    if (inode_of_dir_to_be_remove.i_size>0) {
        printf("directory is not empty\n");
        return -1;
    }

    FreeInode(dev,dp->d_ino);
    //remove dir entry from parent directory
    dp->d_ino=0;

    WriteBlock(dev,blkno,buf);

    return 0;

}



int OpenDir(int pardir, char *dname)
{
    struct DirEntry* parent = dir_table[pardir];
    struct INode inode;
    ReadInode(0,parent->d_entry.d_ino,&inode);

    char buf[BLKSIZE];
    struct OnDiskDirEntry* dp;

    int data_blks_allocated_to_dir= inode.i_size/BLKSIZE + (inode.i_size % BLKSIZE)==0?0:1 ;

    int i=0,done=0;
    while (done==0&&i<data_blks_allocated_to_dir)  // assuming dirctory-file is small(no need to look in indirect,double indirect.)
    {
        int blkno=inode.i_blks[i];
        ReadBlock(dev,DATA_BLK_STARTS_AT+blkno,buf);

        dp= (struct OnDiskDirEntry*) buf;
        int j=0;
        while (done==0 && j<BLKSIZE/sizeof (struct OnDiskDirEntry)) {
            if (strcmp (dp->d_name,dname)==0) {
                done = 1;
                continue;
            }
            j++;dp++;
        }
        i++;
    }

    //get a free entry in dir table
    i=0;
    while (dir_table[i]!=NULL&&i<10)
        i++;
    if (i>=10) {
        printf("can't open more than 10 directories\n");
        return -1;
    }

    dir_table[i]= (struct DirEntry*) malloc(sizeof(struct DirEntry));
    dir_table[i]->d_offset=0;
    my_memcpy(&dir_table[i]->d_entry, dp,sizeof (struct OnDiskDirEntry));

    return i;
}



int CloseDir(int dirhandle)
{
    if (dir_table[dirhandle]==NULL)
        return -1;

    free(dir_table[dirhandle]);
    dir_table[dirhandle]=NULL;
    return 0;
}



int SeekDir(int dirhandle, int pos, int whence)
{
    if (dir_table[dirhandle]==NULL)
        return -1;

    if (whence==SEEK_SET) {
        dir_table[dirhandle]->d_offset = pos;
    }
    else if (whence==SEEK_CUR) {
        dir_table[dirhandle]->d_offset += pos;
    }
    else if(whence==SEEK_END) {

        struct INode inode;
        int dev=0;
        ReadInode(dev,dir_table[dirhandle]->d_entry.d_ino, &inode);

        dir_table[dirhandle]->d_offset = inode.i_size+pos;

    }
    return dir_table[dirhandle]->d_offset;
}





int ReadDir(int dirhandle, struct DirEntry *dent)
{
    if (dirhandle<0||dirhandle>=10)
        return -1;

    struct INode inode;
    int  dev=0;
    ReadInode(dev,dir_table[dirhandle]->d_entry.d_ino, &inode);

    int dentries_in_one_block = BLKSIZE / (sizeof (struct OnDiskDirEntry));


    int blk_count= dir_table[dirhandle]->d_offset / dentries_in_one_block;

    char buf [BLKSIZE];
    ReadBlock(dev,DATA_BLK_STARTS_AT+inode.i_blks[blk_count],buf);

    struct OnDiskDirEntry* dp= (struct OnDiskDirEntry*) buf;

    dp += dir_table[dirhandle]->d_offset % dentries_in_one_block;

    my_memcpy(&dent->d_entry, dp, sizeof (struct OnDiskDirEntry) );
    dent->d_offset =0;

    dir_table[dirhandle]->d_offset++;

    return 0 ;// success

}




int WriteDir(int dirhandle, struct DirEntry *dent)
{
    if (dirhandle<0||dirhandle>=10)
        return -1;

    struct INode inode;
    int  dev=0;
    ReadInode(dev,dir_table[dirhandle]->d_entry.d_ino, &inode);

    int dentries_in_one_block = BLKSIZE / (sizeof (struct OnDiskDirEntry));


    int blk_count= dir_table[dirhandle]->d_offset / dentries_in_one_block;

    char buf [BLKSIZE];

    ReadBlock(dev,DATA_BLK_STARTS_AT+inode.i_blks[blk_count],buf);

    struct OnDiskDirEntry* dp= (struct OnDiskDirEntry*) buf;

    dp += dir_table[dirhandle]->d_offset % dentries_in_one_block;

    my_memcpy(  dp, &dent->d_entry, sizeof (struct OnDiskDirEntry) );

    WriteBlock(dev,DATA_BLK_STARTS_AT+inode.i_blks[blk_count],buf);

    dir_table[dirhandle]->d_offset++;

    return 0 ;// success

}




//============== UFS INTERNAL LOW LEVEL ALGORITHMS =============


int ReadInode(int dev, int ino, struct INode *inode)
{
    if (inode==NULL)
        return -1;// error

    int ino_in_single_blk= BLKSIZE/sizeof(struct INode);
    int blkno= (ino-1)/ino_in_single_blk;
    int offset = (ino-1)- blkno*ino_in_single_blk;

    char buf[BLKSIZE];

    ReadBlock(dev,INODE_BLK_STARTS_AT+blkno,buf);

    struct INode* ptr= (struct INode*) buf;
    my_memcpy(inode,ptr+offset,sizeof(struct INode));
    return 0;

}



int WriteInode(int dev, int ino, struct INode *inode)
{
    if (inode==NULL)
        return -1;// error

    int ino_in_single_blk= BLKSIZE/sizeof(struct INode);
    int blkno= (ino-1)/ino_in_single_blk;
    int offset = (ino-1)- blkno*ino_in_single_blk;

    char buf[BLKSIZE];

    ReadBlock(dev,INODE_BLK_STARTS_AT+blkno,buf);

    struct INode* ptr= (struct INode*) buf;

    my_memcpy(ptr+offset,inode,sizeof(struct INode));

    WriteBlock(dev,INODE_BLK_STARTS_AT+blkno,buf);

    return 0;
}



int readSuper (int dev, struct SupBlock* sb) {

    if (sb==NULL)
        return -1;//error
    char* ptr = (char*) sb;
    ReadBlock(dev,SUP_BLK_STARTS_AT+0,ptr);
    ReadBlock(dev,SUP_BLK_STARTS_AT+1,ptr+BLKSIZE);
    return 0;//success
}




int writeSuper (int dev,struct SupBlock* sb) {

    if (sb==NULL)
        return -1;// error
    char* ptr=(char*)sb;
    WriteBlock( dev,SUP_BLK_STARTS_AT+0,ptr);
    WriteBlock(dev,SUP_BLK_STARTS_AT+1,ptr+BLKSIZE);
    return 0;
}



int AllocInode(int dev)
{
    char buf[BLKSIZE * 2];

    struct SupBlock* sptr= (struct SupBlock*) buf;
    readSuper(dev,sptr);

    if (sptr->sb_nfreeino==0) {
        //no free inos available
        return -1;
    }
    sptr->sb_nfreeino--;

    if (sptr->sb_freeinoindex==(BLKSIZE-54)/sizeof(short))//if list becomes empty
    {
        // then scan the inode block to search free inodes and fill again the list in superblk
        int i;
        char buff[BLKSIZE];
        int done=0;
        for (i=0; i< 8 && done==0 ; i++)
        {
            ReadBlock(dev,INODE_BLK_STARTS_AT+i,buff);
            struct INode* iptr=(struct INode*) buff;
            int j;
            for (j=0; j< BLKSIZE/sizeof(struct INode) && done==0 ; j++,iptr++)
            {
                if (iptr->i_lnk ==0) // no of hardlinks==0
                {
                    int ino_of_this_inode = i* (BLKSIZE/sizeof(struct INode)) + j + 1;

                    sptr->sb_freeinoindex--;
                    sptr->sb_freeinos[sptr->sb_freeinoindex]=ino_of_this_inode;

                    // if list is full now
                    if (sptr->sb_freeinoindex==0)
                        done=1;
                }
            }
        }
    }


    int free_inode= sptr->sb_freeinos[sptr->sb_freeinoindex];

    // Freeze the changes in super block
    sptr->sb_freeinoindex++;
    writeSuper(dev,buf);

    return free_inode;

}




int FreeInode(int dev, int ino)
{
    char buf[BLKSIZE * 2];
    ReadBlock(dev,SUP_BLK_STARTS_AT,buf);
    ReadBlock( dev,SUP_BLK_STARTS_AT+1,buf+BLKSIZE);
    struct SupBlock* sptr= (struct SupBlock*) buf;

    sptr->sb_nfreeino++;

    if (sptr->sb_freeinoindex !=0 ) // if inode list is not full
    {
        sptr->sb_freeinoindex--;
        sptr->sb_freeinos[sptr->sb_freeinoindex]= ino;
    }


    writeSuper(dev,sptr);

    struct INode inode;
    ReadInode(dev,ino,&inode);
    inode.i_lnk=0;
    WriteInode(dev,ino,&inode);
    //return success
    return 0;
}




int AllocBlk(int dev)
{
    char buf[BLKSIZE * 2];
    struct SupBlock* sptr= (struct SupBlock*) buf;
    readSuper(dev,buf);

    if (sptr->sb_nfreeblk==0) {
        //no free blks available
        return -1;
    }
    sptr->sb_nfreeblk--;
    if (sptr->sb_freeblkindex==BLKSIZE/sizeof(short))//if it is the last entry
     {
        // this will be executed if super-blk free-blk-list becoms empty..now copy the whole block pointed by pointer in list
        // at index 0 to the super-blk free-blk list
        int blkno=sptr->sb_freeblks[0];
        ReadBlock(dev,DATA_BLK_STARTS_AT+blkno,sptr->sb_freeblks);
        //update superblk and freeze the changes
        sptr->sb_freeblkindex=1;
        writeSuper(dev,sptr);
        //now return this block as free block
         return blkno;

    }

    int freeblk=sptr->sb_freeblks[sptr->sb_freeblkindex];
    sptr->sb_freeblkindex++;
    // Freeze the changes in super-block
    writeSuper(dev,sptr);
    return freeblk;



}





int FreeBlk(int dev, int blk)

{
    char buf[BLKSIZE * 2];
    struct SupBlock* sptr= (struct SupBlock*) buf;

    readSuper(dev,sptr);

    sptr->sb_nfreeblk++;

    //if list is already full
    if (sptr->sb_freeblkindex==1)
    {
        //then copy (empty) the list into given blk and store its address at index 0 in the list
        WriteBlock(dev,DATA_BLK_STARTS_AT+blk,(char*)sptr->sb_freeblks);
        sptr->sb_freeblks[0]=blk;
        sptr->sb_freeblkindex= BLKSIZE/sizeof(short);
        writeSuper(dev,sptr);

        return 0;//success
    }
    else {
        sptr->sb_freeblkindex--;
        sptr->sb_freeblks[sptr->sb_freeblkindex]=blk;
        writeSuper(dev,sptr);

    }

    return 0;//success

}

//============== DEVICE DRIVER LEVEL =====================



// Reading a logical block blk from device dev

int ReadBlock(int dev, int blk, char buf[BLKSIZE])
{
	// Check for validity of the block
        if (blk<0||blk > 8192)
            return -1;
	// Check for validity of the device
        if (devfd[dev] < 0 || devfd[dev]<0)
            return -1;
	// If OK read the block
	lseek(devfd[dev], blk * BLKSIZE, SEEK_SET);
	return read(devfd[dev], buf, BLKSIZE);
}



// Writing a logical block blk to device dev
int WriteBlock(int dev, int blk,char buf[BLKSIZE])
{
	
	// Check for validity of the block
        if (blk<0||blk > 8192)
            return -1;
	// Check for validity of the device
        if (dev < 0 || devfd[dev]<0)
            return -1;

	// If OK write the block
	lseek(devfd[dev], blk * BLKSIZE, SEEK_SET);
	return write(devfd[dev], buf, BLKSIZE);

}




// Open the device
int OpenDevice(int dev)
{
	// Open the device related file for both reading and writing.

	if ((devfd[dev] = open("new_disk", 0755)) < 0)
	{
		perror("Opening device file failure:");
		exit(0);
	}
	return devfd[dev];
}




// Shutdown the device
int ShutdownDevice(int dev)
{
    if (dev >=0)

	if (devfd[dev] >= 0)
		close(devfd[dev]);

	return 0;
}
