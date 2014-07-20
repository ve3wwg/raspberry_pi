/*********************************************************************
 * Implement a precision "timed wait".  The parameter early_usec
 * allows an interrupted select(2) call to consider the wait as
 * completed, when interrupted with only "early_usec" left remaining.
 *********************************************************************/
static void
timed_wait(long sec,long usec,long early_usec) {
    fd_set mt;
    struct timeval timeout;
    int rc;

    FD_ZERO(&mt);
    timeout.tv_sec = sec;
    timeout.tv_usec = usec;
    do  {
        rc = select(0,&mt,&mt,&mt,&timeout);
        if ( !timeout.tv_sec && timeout.tv_usec < early_usec )
            return;     /* Wait is good enough, exit */
    } while ( rc < 0 && timeout.tv_sec && timeout.tv_usec );
}

/*********************************************************************
 * End timed_wait.c - by Warren Gay
 * Mastering the Raspberry Pi - ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
