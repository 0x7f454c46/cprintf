#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

void __puts(const char *str)
{
	fputs(str, stdout);
}

void __putlong(long num)
{
	int neg = 0;
	char buf[22], *s;

	buf[21] = '\0';
	s = &buf[20];

	if (num < 0) {
		neg = 1;
		num = -num;
	} else if (num == 0) {
		*s = '0';
		s--;
		goto done;
	}

	while (num > 0) {
		*s = (num % 10) + '0';
		s--;
		num /= 10;
	}

	if (neg) {
		*s = '-';
		s--;
	}
done:
	s++;
	fputs(s, stdout);
}

void __putshort(short num)
{
	int neg = 0;
	char buf[12], *s;

	buf[11] = '\0';
	s = &buf[10];

	if (num < 0) {
		neg = 1;
		num = -num;
	} else if (num == 0) {
		*s = '0';
		s--;
		goto done;
	}

	while (num > 0) {
		*s = (num % 10) + '0';
		s--;
		num /= 10;
	}

	if (neg) {
		*s = '-';
		s--;
	}
done:
	s++;
	fputs(s, stdout);
}

void __putulong(unsigned long num)
{
	/* XXX: make ul version */
	__putlong((long)num);
}

void __putwrite(const char *str, size_t size, size_t nmemb)
{
	fwrite(str, size, nmemb, stdout);
}

int timeval_subtract(struct timeval *result,
		struct timeval *a, struct timeval *b)
{
	/* Perform the carry for the later subtraction by updating b. */
	if (a->tv_usec < b->tv_usec) {
		int nsec = (a->tv_usec - b->tv_usec) / 1000000 + 1;
		b->tv_usec -= 1000000 * nsec;
		b->tv_sec += nsec;
	}
	if (a->tv_usec - b->tv_usec > 1000000) {
		int nsec = (a->tv_usec - b->tv_usec) / 1000000;
		b->tv_usec += 1000000 * nsec;
		b->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	   tv_usec is certainly positive. */
	result->tv_sec = a->tv_sec - b->tv_sec;
	result->tv_usec = a->tv_usec - b->tv_usec;

	/* Return 1 if result is negative. */
	return a->tv_sec < b->tv_sec;
}


int main(int argc, char **argv)
{
	size_t niter = 10000000;
	size_t i;
	static const char str1[] = "String1 String1";
	static const char str2[] = "string2 string2 string2";
	struct rusage usage;
	struct timeval system_start, system_end;
	struct timeval user_start, user_end;
	struct timeval user, sys;

	getrusage(RUSAGE_SELF, &usage);
	user_start = usage.ru_utime;
	system_start = usage.ru_stime;

	for (i = 0; i < niter; i++)
		printf("Some message %s %s %c %li %d %lu\n",
				str1, str2, 'c', (long)-4,
				(short)2, (unsigned long)2);

	getrusage(RUSAGE_SELF, &usage);
	user_end = usage.ru_utime;
	system_end = usage.ru_stime;

	timeval_subtract(&user, &user_end, &user_start);
	timeval_subtract(&sys, &system_end, &system_start);

	fprintf(stderr, "user\t%lu.%u\nsys\t%lu.%u\n",
			user.tv_sec, user.tv_usec,
			sys.tv_sec, sys.tv_usec);

	return 0;
}
