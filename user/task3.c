#ifndef TESTS_H
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#endif

int test_random(int print);

int test_seek(int print) {
  int fd = open("testfile.txt", O_RDWR | O_CREATE | O_TRUNC);
  if (fd < 0) {
    printf("error: open test file on seek test\n");
    return 0;
  }

  char expected1[] = "Hello, World!\n";
  char expected2[] = "Hello, World!\n\0This is new data!\n";
  char expected3[] = "ta!\n";
  char expected4[] = "Hello, World!\n\0This is new data!\n\0Extra fresh data!\n";

  // Write some data to the beginning of the file
  seek(fd, 0, SEEK_SET);
  char write1[] = "Hello, World!\n";
  write(fd, write1, sizeof(write1));

  // Seek to the beginning of the file to read and print the written data
  seek(fd, 0, SEEK_SET);
  char actual1[20];
  read(fd, actual1, sizeof(actual1));
  if (memcmp(expected1, actual1, strlen(expected1)) == 0)
  {
    // if (print) 
    //   printf("Writing and reading \"Hello, World!\\n\" successful!\n\n");
  }
  else 
  {
    printf("ERROR: FAILURE at writing or reading \"Hello, World!\\n\"\n\n");
    close(fd);
    return 0;
  }

  // if (print) printf("File content after first write:\n");
  // for(int i = 0; i < sizeof(actual1); i++)
  //   if (print) printf("%c", actual1[i]);
  // if (print) printf("\n");

  // Seek to the end of the file to write additional data
  seek(fd, 0, SEEK_CUR);
  char write2[] = "This is new data!\n";
  write(fd, write2, sizeof(write2));

  // Testing serek with negative offset whose absolute value is larger than filesize
  // should seek to offset 0
  seek(fd, -50, SEEK_CUR);
  
  // Read the updated data from the beginning of the file
  char actual2[40];
  read(fd, actual2, sizeof(actual2));

  if (memcmp(expected2, actual2, sizeof(expected2)) == 0)
  {
    // if (print) printf("Concatenating \"This is new data!\\n\" successful!\n\n");
  }
  else
  {
    printf("ERROR: FAILURE to concatenate or read entire data from the file\n\n");
    close(fd);
    return 0;
  }
  // if (print) printf("File content after second write:\n");
  // for(int i = 0; i < sizeof(actual2); i++)
  //   if (print) printf("%c", actual2[i]);
  // if (print) printf("\n");

  // Seek slightly backwards then re-read
  seek(fd, -5, SEEK_CUR);
  char actual3[40];
  read(fd, actual3, sizeof(actual3));
  if (memcmp(expected3, actual3, sizeof(expected3)) == 0)
  {
    // if (print) printf("Moving backwards 5 bytes then reading is successful!\n\n");
  }
  else 
  {
    printf("ERROR: FAILURE to go back 5 bytes and read content properly\n\n");
    close(fd);
    return 0;
  }
  // if (print) printf("Read after going slightly backwards:\n%s\n", actual3);

  // Go too far to write new stuff
  seek(fd, 500, SEEK_SET);
  char write3[] = "Extra fresh data!\n";
  write(fd, write3, sizeof(write3));

  seek(fd, 0, SEEK_SET);
  char actual4[70];
  read(fd, actual4, sizeof(actual4));

  if (memcmp(expected4, actual4, sizeof(expected4)) == 0)
  {
    // if (print) printf("Concatenating \"Extra fresh data!\\n\" successful!\n\n");
  }
  else 
  {
    printf("ERROR: FAILURE to concatenate or read extra data\n\n");
    close(fd);
    return 0;
  }

  // if (print) printf("File content after third write:\n");
  // for(int i = 0; i < sizeof(actual4); i++)
  //   if (print) printf("%c", actual4[i]);
  // if (print) printf("\n");

  printf("SUCCESS: TEST SEEK successful\n");

  close(fd);
  return 1;
}

uint8 lfsr_char(uint8 lfsr)
{
  uint8 bit;
  bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
  lfsr = (lfsr >> 1) | (bit << 7);
  return lfsr;   
}

int test_first_with_original_seed(int fd)
{
  uint8 res;
  if (read(fd, &res, 1) < 0)
    return 0;

  if (res == lfsr_char(0x2A))
    return 1;
  return 0;
}

int test_looping(int fd)
{
  uint8 prev, res;
  if (read(fd, &prev, 1) < 0)
    return 0;
  uint8 firstRead = prev;

  for(int i = 0; i < 255; i++)
  {
    if (read(fd, &res, 1) < 0)
      return 0;
    if (lfsr_char(prev) != res)
      return 0;
    prev = res;
  }

  if (res != firstRead)
    return 0;

  return 1;
}

int set_seed(int fd, uint8 seed)
{
  return write(fd, &seed, 1);
}

int test_continuity(int fd)
{
  uint8 prev;
  if (read(fd, &prev, 1) < 0)
    return 0;
  if (close(fd) < 0)
    return 0;
  
  if ((fd = open("random", O_RDWR)) < 0)
    return 0;
  
  uint8 res;
  if (read(fd, &res, 1) < 0)
    return 0;

  if (lfsr_char(prev) != res)
    return 0;
  
  return 1;
}

int test_concurrency_read(int fd)
{
  uint8 res, first;
  if (read(fd, &res, 1) < 0)
    return 0;
  first = res;

  if (fork() == 0)
  {
    if (read(fd, &res, 1) < 0)
      return 0;
    if (read(fd, &res, 1) < 0)
      return 0;
    if (read(fd, &res, 1) < 0)
      return 0;
    exit(0);
  }
  else
  {
    if (read(fd, &res, 1) < 0)
      return 0;
    if (read(fd, &res, 1) < 0)
      return 0;
    if (read(fd, &res, 1) < 0)
      return 0;
    if (read(fd, &res, 1) < 0)
      return 0;
  }

  wait(0);
  if (read(fd, &res, 1) < 0)
    return 0;

  if (write(fd, &first, 1) == -1)
    return 0;

  for(int i = 0; i < 8; i++)
    if (read(fd, &first, 1) < 0)
      return 0;

  if (res != first)
    return 0;

  return 1;
}

void reset_seed(int print){
  int fd = open("random", O_RDWR);
  if (fd < 0) {
    printf("ERROR ON openning 'random' file\n");
  }
  uint8 originalSeed = 0x2A;
  int writeResult = write(fd, &originalSeed, 1);
  if (writeResult == -1)
  {
      printf("ERROR ON RESETING TO ORIGINAL SEED\n");
  }
  else if(print) printf("RESET SEED TO THE ORIGINAL SEED!\n");
}

void handleFailure(char *errorMsg, int fd)
{
  printf("%s\n", errorMsg);
  set_seed(fd, 0x2A);
  close(fd);
}

int test_ass4(int argc, char** argv)
{
    // Reset flag = reset PRG to original seed 0x2A
    int reset = 0;
    // print flag = print non-errors during sub-tests
    int print = 0;
    for(int i = 0; i < argc; i++)
    {
        if (strcmp("-r", argv[i]) == 0)
            reset++;
        if (strcmp("-p", argv[i]) == 0)
            print++;
    }
    
    int success = 0;
    // printf("SEEK TESTS:\n~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    success += test_seek(print);
    // printf("~~~~~~~~~~~~~~~~~~~~~~~~~~\n~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    // printf("~~~~~~~~~~~~~~~~~~~~~~~~~~\n~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    // printf("\nPRNG TESTS:\n~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    success += test_random(reset);
    if (success == 2){
        printf("SUCCESS!! PASSED ALL ASS4 TESTS!\n");
        return 1;
    }

    // else
    printf("FAILED 1 OR MORE TESTS\n");
    return 0;
}

int test_concurrency(int print){
  if (fork() == 0) {reset_seed(print);}
  int success = test_random(0);
  if (!success) return -1;


  return 0;
}

int test_concurrency_write(int fd, int print)
{
  uint8 newSeed = 0x38;
  uint8 res;
  if(fork() == 0)
  {
    if (write(fd, &newSeed, 1) == -1)
      return 0;
    exit(0);
  }
  else
  {
    for(int i = 0; i < 10; i++)
    {
      if (read(fd, &res, 1) < 0)
        return 0;
    }
  }
  wait(0);
  if (read(fd, &res, 1) < 0)
    return 0;
  uint8 prev = newSeed;
  for(int i = 0; i < 11; i++)
  {
    if (prev == res)
    {
      if (print) printf("SUCCESS: Passed concurrency read test\n");
      // if (print) printf("\nWrite concurrency test: Reading on new seed occured %d times!\n\n", i);
      return 1;
    }
    prev = lfsr_char(prev);
  }

  if (prev == res)
    return 1;

  return 0;
}

int test_random(int print)
{
  int fd = open("random", O_RDWR | O_CREATE);
  if (fd < 0)
    {
      handleFailure("ERROR: OPENING RANDOM DEVICE", fd);
      return 0;
    }

  if (test_first_with_original_seed(fd) == 0)
    {
      handleFailure("ERROR: Failed reading original seed", fd);
      return 0;
    }
  if (print) printf("SUCCESS: Passed reading original seed\n");

  if (test_looping(fd) == 0)
  {
    handleFailure("ERROR: Failed looping 255 to return to original seed", fd);
    return 0;
  }
  if (print) printf("SUCCESS: Passed looping to regenerate original seed\n");

  if (set_seed(fd, 0x4C) == -1)
    {
    handleFailure("ERROR: Failed writing a new seed", fd);
    return 0;
  }
  if (print) printf("SUCCESS: Passed writing a new seed\n");

  if (test_looping(fd) == 0)
  {
    handleFailure("ERROR: Failed looping 255 to return to new seed", fd);
    return 0;
  }
  if (print) printf("SUCCESS: Passed looping to regenerate new seed\n");

  if (test_continuity(fd) == 0)
  {
    handleFailure("ERROR: Failed to continue generating from point before closing the device", fd);
    return 0;
  }
  if (print) printf("SUCCESS: Passed continuity test to continue generating seed from point before closing the device\n");

  if (test_concurrency_read(fd) == 0)
  {
    handleFailure("ERROR: Failed reading proper values concurrently", fd);
    return 0;
  }
  if (print) printf("SUCCESS: Passed generating proper values concurrently\n");

  if (test_concurrency_write(fd, print) == 0)
  {
    handleFailure("ERROR: Failed writing new seed concurrently", fd);
    return 0;
  }
  if (print) printf("SUCCESS: Passed writing new seed concurrently\n");

  if (set_seed(fd, 0x2A) == -1)
  {
    handleFailure("ERROR: Failed resetting to original seed", fd);
    return 0;
  }
  close(fd);
  printf("SUCCESS! Passed all RANDOM tests!\n");
  return 1;
}



// -------------------- MAIN ----------------------

int
main(int argc, char *argv[])
{
  test_random(1);

  return 0;
}