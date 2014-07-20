/*********************************************************************
 * Mutex.c 
 *********************************************************************/

static pthread_mutex_t mutex;

static void
mutex_init(void) {
	int rc = pthread_mutex_init(&mutex,0);
	assert(!rc);
}

static void
mutex_lock(void) {
	int rc = pthread_mutex_lock(&mutex);
	assert(!rc);
}

static void
mutex_unlock(void) {
	int rc = pthread_mutex_unlock(&mutex);
	assert(!rc);
}

/*********************************************************************
 * End mutex.c - by Warren Gay
 * Mastering the Raspberry Pi, ISBN13: 978-1-484201-82-4
 * This source code is placed into the public domain.
 *********************************************************************/
