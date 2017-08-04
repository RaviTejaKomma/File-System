#include "FunctionHeaders.h"

/*
FIle size --> 20971520
Master block 0 - 127 => 128 butes
Users data 128 - 2687 => 2560 bytes
Bit vector 2688 - 35455 => 32768 bytes

Meta message 35456 - 4229759
Message data 4229760 - 8424063

File Bit Vector 8424064 - 8608383
File meta data 8608384 - 8870527
File actual content 8870528 ----
*/

#define message_content_start 4229760
#define message_block_size 128
#define file_block_size 2048
#define first_file_position 8870528

typedef struct
{
	long long int userpos;   // points to the first user
	long long int message_bit_vector_pos; // points to the bit vector
	long long int metamsgpos; // points to the metamsg
	long long int usercount;   // number count address
	long long int message_bit_vector_count;

	long long int filemetapos;
	long long int filemetacount;
	long long int file_bit_vector_pos;
	long long int file_bit_vector_count;
	int pad[14];
}masterblock;

typedef struct{
	char filename[16];
	char username[16];
	int fileid;
	long long int filesize;
	long long int no_of_blocks;
	long long int bit_vect_start_pos; // starting bit vetcor block of the file
	int is_Active;
	int pad[16];
}filemetadata;

typedef struct
{
	char username[16];
	char password[16];
	long long int first_meta_messageid;
	long long int last_meta_messageid;
	int pad[20];
}users;

typedef struct
{
	char username[16];
	char filename[16];
	long long int msgpos;
	int pad[22];
}msgmetadata;

typedef struct
{
	char bits[128]; // if message is present it sets
}message_bit_vector;

typedef struct
{
	char msg[128];   // actual message content
}actualmsgdata;

typedef struct
{
	char file[2048];   // actual message content
}actualfiledata;

int file_bit_vector[46080];  // file bit vector

void * allocate(int block_size){
	return calloc(1, block_size);
}

void write_block(FILE *fp, int offset, void *memory, int block_size){
	fseek(fp, offset, SEEK_SET);
	fwrite(memory, block_size, 1, fp);
}

void* read_block(FILE *fp, int offset, int block_size){
	fseek(fp, offset, SEEK_SET);
	void *memory = allocate(block_size);
	fread(memory, block_size, 1, fp);
	return memory;
}

void initialize_file_bit_vector(FILE *fp, masterblock *master){
	fseek(fp, master->file_bit_vector_pos, SEEK_SET);
	memset(file_bit_vector, 0, sizeof(file_bit_vector));
	fwrite(file_bit_vector, sizeof(int), 46080, fp);
	return;
}

void initialize_bit_vector(FILE *fp, masterblock *master){
	message_bit_vector *temp;
	int first_message_bit_vector_address = master->message_bit_vector_pos;
	while (first_message_bit_vector_address < master->metamsgpos){
		temp = (message_bit_vector*)allocate(message_block_size);
		for (int i = 0; i < message_block_size; i++){
			temp->bits[i] = '0';
		}
		write_block(fp, first_message_bit_vector_address, temp, message_block_size);
		first_message_bit_vector_address = first_message_bit_vector_address + message_block_size;
	}
}

masterblock *loadmasterblock(FILE *fp)
{
	masterblock *mb = NULL;
	mb = (masterblock*)read_block(fp, 0, message_block_size);
	if (mb->userpos == 0)  // when initially file is empty no user will be present
	{
		masterblock *mb1 = (masterblock*)allocate(message_block_size);
		mb1->userpos = 128;
		mb1->usercount = 0;

		mb1->message_bit_vector_pos = 2688;
		mb1->metamsgpos = 35456;
		mb->message_bit_vector_count = 0;

		mb1->file_bit_vector_count = 0;
		mb1->file_bit_vector_pos = 8424064;
		mb1->filemetapos = 8608384;
		mb1->filemetacount = 0;

		write_block(fp, 0, mb1, message_block_size);
		initialize_bit_vector(fp, mb1);
		initialize_file_bit_vector(fp, mb1);
	}
	mb = (masterblock*)read_block(fp, 0, message_block_size);  // reading the master again
	printf("%lld %lld %lld %lld %lld %lld %lld %lld %lld\n", mb->userpos, mb->usercount, mb->message_bit_vector_pos, mb->metamsgpos, mb->message_bit_vector_count,
		mb->file_bit_vector_count, mb->file_bit_vector_pos, mb->filemetapos, mb->filemetacount);
	return mb;
}

int user_exists_or_not(FILE *fp, users *newuser, masterblock *master){
	if (fp == NULL)
	{
		printf("File not found");
		return -1;
	}
	users *user_record = NULL;
	int i = 0;
	while (i < master->usercount){
		user_record = (users*)read_block(fp, master->userpos + i*message_block_size, message_block_size);
		if (strcmp(user_record->username, newuser->username) == 0){     // username matchhes
			if (strcmp(user_record->password, newuser->password) == 0){
				return i + 1; // if user_exists and password matches then return the index of that user(i.e number of that user)
			}
			else{
				return 0; // password doesnot match   
			}
		}
		i++;
	}
	return 0;
}

void menu(){
	printf("\n----------------------------------------------------------------------------------------------------------------\n");
	printf("\nEnter 1 to create message");
	printf("\nEnter 2 to read message");
	printf("\nEnter 3 to uploadfile");
	printf("\nEnter 4 to downloadfile");
	printf("\nEnter 5 to delete message");
	printf("\nEnter 6 to enumerate all the messages");
	printf("\nEnter 7 to view all the images uploaded by you");
	printf("\nEnter 8 to delete file");
	printf("\nEnter -1 logout\n");
}

void view_all_images(FILE *fp, masterblock *master, users *user){
	filemetadata *filemeta;

	printf("\nDear %s these are the files that you have uploaded\n", user->username);
	for (int i = 0; i < master->filemetacount; i++){
		filemeta = (filemetadata*)read_block(fp, master->filemetapos + i*message_block_size, message_block_size);
		if (strcmp(filemeta->username, user->username) == 0 && filemeta->is_Active == 1)
		{
			printf("\nFilename is %s ", filemeta->filename);
		}
	}
	return;
}

long long int get_first_message_bit_vector_zero(FILE *fp, masterblock *master)
{
	// searching for the first zero index //
	message_bit_vector *temp;
	int first_message_bit_vector_address = master->message_bit_vector_pos;
	while (first_message_bit_vector_address < master->metamsgpos){
		temp = (message_bit_vector*)read_block(fp, first_message_bit_vector_address, message_block_size);
		for (int i = 0; i < message_block_size; i++){
			if (temp->bits[i] == '0')
			{
				temp->bits[i] = '1';
				master->message_bit_vector_count += 1;
				write_block(fp, 0, master, message_block_size);  // updating the bit vector count in master block
				fseek(fp, first_message_bit_vector_address + i, SEEK_SET);
				fwrite(&(temp->bits[i]), 1, 1, fp);
				return (first_message_bit_vector_address - master->message_bit_vector_pos) + i;
			}
		}
		first_message_bit_vector_address = first_message_bit_vector_address + message_block_size;
	}
	// searching for the first zero index //
	return -1; // message space is filled
}

void createMessage(FILE *fp, masterblock *master, users *user){
	actualmsgdata *message = (actualmsgdata*)allocate(message_block_size);
	msgmetadata *msgmeta = (msgmetadata*)allocate(message_block_size);

	// filling message meta data and message data//
	printf("\nEnter the file name on which you want to comment : ");
	scanf("%s", msgmeta->filename);     // filename should not contain spaces
	strcpy(msgmeta->username, user->username);
	printf("\nEnter the comment : ");
	fflush(stdin);
	//char buffer[128];
	//fgets(buffer,message_block_size, stdin);
	//buffer[strlen(buffer) - 1] = '\0';
	//strcpy(message->msg, buffer);
	scanf("%[^\t\n]", message->msg);

	long long int index = get_first_message_bit_vector_zero(fp, master);  // where the first zero occured its index
	if (index == -1)
	{
		printf("\n-----------Message data exhausted-------------");
		return;
	}
	msgmeta->msgpos = message_content_start + index*message_block_size;

	// writing the metamsg //
	write_block(fp, master->metamsgpos + index * 128, msgmeta, message_block_size);
	// writing the message // 
	write_block(fp, message_content_start + index * 128, message, message_block_size);
	printf("\n-----------Message added successfully-------------");
	return;
}

void readMessage(FILE *fp, masterblock *master, users *user){
	char filename[16];
	printf("\nEnter the filename to which you want to view the comments : ");
	scanf("%s", filename);

	char ch;
	msgmetadata *metamsg = NULL;
	actualmsgdata *message = NULL;
	for (int i = 0; i < master->message_bit_vector_count; i++)
	{
		fseek(fp, (master->message_bit_vector_pos) + i * 1, SEEK_SET);
		ch = fgetc(fp);
		if (ch == '1'){
			metamsg = (msgmetadata*)allocate(message_block_size);
			metamsg = (msgmetadata*)read_block(fp, master->metamsgpos + i*message_block_size, message_block_size);
			if (strcmp(metamsg->filename, filename) == 0){
				message = (actualmsgdata*)allocate(message_block_size);
				message = (actualmsgdata*)read_block(fp, metamsg->msgpos, message_block_size);
				printf("\nMessage is : %s, author is : %s", message->msg, metamsg->username);
			}
		}
	}
	return;
}

void upload_file(FILE *fp, masterblock *master, users *user){
	char filename[32];
	printf("\nEnter the name of the file : ");
	scanf("%s", filename);
	FILE *output = fopen(filename, "rb+");

	filemetadata file_meta_data;
	long long int first_bit_vector_position = master->file_bit_vector_pos;

	// if another file comes with the same name
	/*for (int i = 0; i < master->filemetacount; i++){
	file_meta_data = (filemetadata*)read_block(fp, master->filemetapos + i*message_block_size, message_block_size);
	if (!strcmp(filename, file_meta_data->filename)){
	first_bit_vector_position = file_meta_data->bit_vect_start_pos;
	break;
	}
	}*/


	// calculating the size of the file // 
	fseek(output, 0, SEEK_END);
	int size = ftell(output);
	fseek(output, 0, SEEK_SET);
	// calculating the size of the file // 

	int no_of_blocks = size / 2048;  // not exactly mutliple of 2kb
	if (size % 2048 != 0){
		no_of_blocks++;
	}

	file_meta_data.fileid = master->filemetacount + 1;
	file_meta_data.filesize = size;
	file_meta_data.is_Active = 1;
	file_meta_data.no_of_blocks = no_of_blocks;
	strcpy(file_meta_data.filename, filename);
	strcpy(file_meta_data.username, user->username);

	int flag = 0; no_of_blocks = 0;

	fseek(fp, first_bit_vector_position, SEEK_SET);
	fread(&file_bit_vector, sizeof(int), 46080, fp);

	for (int i = 0; i < 46080; i++){
		if (no_of_blocks == file_meta_data.no_of_blocks)
			break;
		if (file_bit_vector[i] == 0)
		{
			if (flag == 0){
				file_meta_data.bit_vect_start_pos = i;  // to get the first bit vector index of the file
				flag = 1;
			}
			file_bit_vector[i] = file_meta_data.fileid;
			int index = first_file_position + i*file_block_size;
			fseek(fp, index, SEEK_SET);
			no_of_blocks++;
			int count = 0;
			while (count != 2048 && !feof(output))
			{
				char temp = fgetc(output);
				fwrite(&temp, sizeof(char), 1, fp);
				count += 1;
			}
		}
	}

	// write the meta message block //
	write_block(fp, master->filemetapos + master->filemetacount*message_block_size, &file_meta_data, message_block_size);
	// write the meta message block //

	// update the master block //
	master->filemetacount += 1;
	write_block(fp, 0, master, message_block_size);
	// update the master block //

	// writing the bit vector //
	fseek(fp, master->file_bit_vector_pos, SEEK_SET);
	fwrite(file_bit_vector, sizeof(int), 46080, fp);
	// writing the bit vector //
	printf("\nFile uploaded successfully\n");
	fclose(output);
}

void download_file(FILE *fp, masterblock *master, users *user){

	printf("\nEnter the path of the file name : ");
	fflush(stdin);
	char filename[16];
	scanf("%s", filename);

	char output_filename[30] = "output_";
	strcat(output_filename, filename);
	FILE *output = fopen(output_filename, "wb");

	filemetadata *file_meta_data;

	// loading the bit vector into the ram //
	long long int first_bit_vector_position = master->file_bit_vector_pos;
	fseek(fp, first_bit_vector_position, SEEK_SET);
	fread(&file_bit_vector, sizeof(int), 46080, fp);
	// loading the bit vector into the ram //

	for (int i = 0; i < master->filemetacount; i++)
	{
		file_meta_data = (filemetadata*)read_block(fp, master->filemetapos + i*message_block_size, message_block_size);
		if (strcmp(filename, file_meta_data->filename) == 0)
		{
			break;
		}
	}

	int no_of_blocks = 0, count;
	first_bit_vector_position = 0;
	fseek(output, 0, SEEK_SET);
	char temp;

	for (int i = first_bit_vector_position; i < 46080; i++)
	{
		if (no_of_blocks == file_meta_data->no_of_blocks)
		{
			break;
		}
		if (file_bit_vector[i] == file_meta_data->fileid)
		{
			int index = first_file_position + i*file_block_size;
			fseek(fp, index, SEEK_SET);
			no_of_blocks++;
			count = 0;
			while (count != 2048 && !feof(output))
			{
				temp = fgetc(fp);
				fwrite(&temp, sizeof(char), 1, output);
				count += 1;
			}
		}
	}
	fclose(output);
	system(output_filename); // to open the file when it is closed
	printf("\nFile downloaded successfully\n");
	return;
}

void delete_file(FILE *fp, masterblock *master, users *user){

	printf("\nDear %s these are the files that you have uploaded\n", user->username);
	filemetadata *metafile = NULL;
	int i = 0;
	for (i = 0; i < master->filemetacount; i++){
		metafile = (filemetadata*)read_block(fp, master->filemetapos + i*message_block_size, message_block_size);
		if (strcmp(metafile->username, user->username) == 0 && metafile->is_Active == 1)
		{
			printf("\nFile id is %d ,Filename is %s ", i, metafile->filename);
		}
	}

	printf("\nEnter any of the file id that you want to delete : ");
	scanf("%d", &i);
	// retrieving the metafile data //
	metafile = (filemetadata*)read_block(fp, master->filemetapos + i*message_block_size, message_block_size);
	metafile->is_Active = 0;
	write_block(fp, master->filemetapos + i*message_block_size, metafile, message_block_size);

	// loading the bit vector into the ram //
	long long int first_bit_vector_position = master->file_bit_vector_pos;
	fseek(fp, first_bit_vector_position, SEEK_SET);
	fread(&file_bit_vector, sizeof(int), 46080, fp);
	// loading the bit vector into the ram //

	int count = 0;
	for (int i = 0; i < 46080; i++){
		if (count == metafile->no_of_blocks)
			break;
		if (metafile->fileid == file_bit_vector[i]){
			file_bit_vector[i] = 0;
			count++;
		}
	}

	printf("\nFile deleted successfully\n");

	// writing the bit vector //
	fseek(fp, master->file_bit_vector_pos, SEEK_SET);
	fwrite(file_bit_vector, sizeof(int), 46080, fp);
	return;
}

void delete_the_message(FILE *fp, masterblock *master, users *user)
{
	printf("\nEnter the name of the file name : ");
	fflush(stdin);
	char filename[16];
	scanf("%s", filename);

	msgmetadata *metamsg = NULL;
	actualmsgdata *message = NULL;
	char ch;
	int i = 0;
	for (i = 0; i < master->message_bit_vector_count; i++)
	{
		fseek(fp, (master->message_bit_vector_pos) + i * 1, SEEK_SET);
		ch = fgetc(fp);
		if (ch == '1'){
			//metamsg = (msgmetadata*)allocate();
			metamsg = (msgmetadata*)read_block(fp, master->metamsgpos + i*message_block_size, message_block_size);
			if (strcmp(metamsg->username, user->username) == 0 && strcmp(metamsg->filename, filename) == 0){
				message = (actualmsgdata*)allocate(message_block_size);
				message = (actualmsgdata*)read_block(fp, metamsg->msgpos, message_block_size);
				printf("\nid : %d ,Message is : %s", i, message->msg);
			}
		}
	}
	printf("\nEnter any of the message id that you want to delete : ");
	scanf("%d", &i);
	fseek(fp, (master->message_bit_vector_pos) + i, SEEK_SET);
	ch = '0';
	fwrite(&ch, sizeof(char), 1, fp);
	printf("\nMessage deleted successfully\n");
	return;
}

void enumerate_the_messages(FILE *fp, masterblock *master, users *user){
	msgmetadata *metamsg = NULL;
	actualmsgdata *message = NULL;
	char ch;
	for (int i = 0; i < master->message_bit_vector_count; i++){
		fseek(fp, (master->message_bit_vector_pos) + i * 1, SEEK_SET);
		ch = fgetc(fp);
		if (ch == '1'){
			metamsg = (msgmetadata*)read_block(fp, master->metamsgpos + i*message_block_size, message_block_size);
			message = (actualmsgdata*)allocate(message_block_size);
			message = (actualmsgdata*)read_block(fp, metamsg->msgpos, message_block_size);
			printf("\nFilename : %s ,message is : %s ,author is : %s\n", metamsg->filename, message->msg, metamsg->username);
		}
	}
	return;
}

void login(FILE *fp, masterblock *master){
	users *user = (users*)allocate(message_block_size);
	// Reading the username //
	printf("\nEnter the Username(Maximum 16 characters) : ");
	scanf("%s", user->username);
	// Reading the username //

	// Reading the password //
	printf("\nEnter the Password(Maximum 16 characters) : ");
	scanf("%s", user->password);
	// Reading the password //

	int user_id = user_exists_or_not(fp, user, master);

	// validating the user //
	if (user_id == 0){
		printf("\nInvalid Username or Password, please try again");
		return;
	}
	else if (user_id != 0){
		printf("\n\t\t\t\t\t   ------ You are successfully logged in ------\t\t\t\t\t\n");
	}
	// validating the user //

	int option = 0;
	while (1){
		menu();
		printf("\n>>");
		scanf("%d", &option);
		switch (option){
		case 1: createMessage(fp, master, user);
			break;
		case 2: readMessage(fp, master, user);
			break;
		case 3: upload_file(fp, master, user);
			break;
		case 4: download_file(fp, master, user);
			break;
		case 5: delete_the_message(fp, master, user);
			break;
		case 6: enumerate_the_messages(fp, master, user);
			break;
		case 7: view_all_images(fp, master, user);
			break;
		case 8: delete_file(fp, master, user);
			break;
		case -1: goto end_of_session;
			break;
		}
	}
end_of_session:
	return;
}

void signup(FILE *fp, masterblock *master){

	users *newuser = (users*)allocate(message_block_size);
	// Reading the username //
	printf("\nEnter the Username(Maximum 16 characters) : ");
	scanf("%s", newuser->username);
	// Reading the username //

	// Reading the password //
	printf("\nEnter the Password(Maximum 16 characters) : ");
	scanf("%s", newuser->password);
	// Reading the password //
	newuser->first_meta_messageid = -1;
	newuser->last_meta_messageid = -1;

	int validate = 0, i = 0;
	users *user_record = NULL;
	while (i < master->usercount){
		user_record = (users*)read_block(fp, master->userpos + i*message_block_size, message_block_size);
		if (strcmp(user_record->username, newuser->username) == 0){     // user already exists
			validate = 1;
		}
		i++;
	}

	if (validate == 1){
		printf("\nUsername already exists, please try again ");
	}
	else{
		write_block(fp, master->userpos + (master->usercount*message_block_size), newuser, message_block_size);  // newuser added successfully
		printf("\nDear %s you are registered successfully, please login to continue", newuser->username);
		// should update the user count as newuser added //
		master->usercount += 1;
		write_block(fp, 0, master, message_block_size); // user count updated successfully
	}
	return;
}

int main()
{
	FILE *fp;
	fp = fopen("database.dat", "rb+");
	masterblock *master = (masterblock*)allocate(message_block_size);
	master = loadmasterblock(fp);

	int login_or_signup = 0;
	while (1){
	start_option: printf("\n\n\t\t\t\t------Enter 0 for SignUp, 1 for LogIn, 2 for Exit Application------\t\t\t\t\n");
		printf(">> ");
		scanf("%d", &login_or_signup);
		if (login_or_signup == 2){
			break;
		}
		switch (login_or_signup)
		{
		case 0: signup(fp, master);
			break;
		case 1: login(fp, master);
			break;
		case 2: goto ending;
			break;
		default: printf("\n Invalid option, please try again");
			goto start_option;
			break;
		}
	}
ending: fclose(fp);
	getchar();
	return 0;
}