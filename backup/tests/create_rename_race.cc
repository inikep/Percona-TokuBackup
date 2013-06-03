/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:  
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "backup_test_helpers.h"
#include "backup_debug.h"

const int N=10;
const int N_FNAMES = 2;

void* do_backups(void *v) {
    check(v==NULL);
    pthread_t thread;
    start_backup_thread(&thread);
    finish_backup_thread(thread);
    return v;
}

char *fnames[N_FNAMES];

void* do_create(void *p) {
    check(p == NULL);
    char * name0 = fnames[0];
    int fd = open(name0, O_RDWR | O_CREAT, 0777);
    if (fd >= 0) {
        int r = close(fd);
        check(r == 0);
    }
    return p;
}

void* do_rename(void *p) {
    check(p == NULL);
    char * name0 = fnames[0];
    char * name1 = fnames[1];
    int r = rename(name0, name1);
    check(r == 0);
    return p;
}

int test_main(int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int r = 0;
    setup_source();
    setup_destination();
    char *src = get_src();
    int len = strlen(src)+10;
    for (int i=0; i<N_FNAMES; i++) {
        fnames[i] = (char*)malloc(len);
        char letter = 'A';
        r = snprintf(fnames[i], len, "%s/%c", src, letter + (char)i);
        check(r < len);
    }

    // 1. Set pause point
    HotBackup::toggle_pause_point(HotBackup::OPEN_DESTINATION_FILE);
    
    // 2. Set backup to keep capturing.
    backup_set_keep_capturing(true);

    // 3. Start backup.
    pthread_t backup_thread;
    start_backup_thread(&backup_thread);

    // 4. Start create() thread.
    pthread_t create_thread;
    r = pthread_create(&create_thread, NULL, do_create, NULL);
    check(r == 0);
    
    // 5. Start rename() thread. (Should block, in fixed version).
    pthread_t rename_thread;
    r = pthread_create(&rename_thread, NULL, do_rename, NULL);
    check(r == 0);

    // 6. Sleep, then turn off pause point.
    // sleep(1);
    HotBackup::toggle_pause_point(HotBackup::OPEN_DESTINATION_FILE);

    // 7. Wait for create() then rename() threads to finish.
    r = pthread_join(create_thread, NULL);
    check(r == 0);
    r = pthread_join(rename_thread, NULL);
    check(r == 0);
    printf("threads done...\n");

    // 8. allow backup to finish (turn keep_capturing off).
    backup_set_keep_capturing(false);

    // 9. Wait for backup to finish.
    finish_backup_thread(backup_thread);

    // 10. Verify that the source file and destination file 
    char *dst = get_dst();
    int dst_len = strlen(dst)+10;
    char * dest_name = (char*)malloc(dst_len);
    char letter = 'B';
    r = snprintf(dest_name, dst_len, "%s/%c", dst, letter);
    check(r < len);

    struct stat buf;
    r = stat(dest_name, &buf);
    if (r != 0) {
        fail();
        perror("Destination file could not be stat()'d");
    } else {
        pass(); printf("\n");
    }

    // Cleanup.
    free(dest_name);
    free(dst);

    for (int i=0; i<N_FNAMES; i++) {
        free(fnames[i]);
    }

    cleanup_dirs();
    free((void*)src);
    return r;
}
