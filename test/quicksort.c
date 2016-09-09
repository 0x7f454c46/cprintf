#include <stdio.h>

/* not a macro, not static, not inline just for test */
void swap(int *a, int *b)
{
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

int partition(int *arr, int l, int h)
{
	int p = arr[h]; /* pivot element */
	int i = (l - 1);
	int j;

	for (j = l; j <= h - 1; j++) {
		if (arr[j] <= p) {
			i++; /* index of the lastest small element */
			swap(&arr[i], &arr[j]);
		}
	}
	swap(&arr[i + 1], &arr[h]); /* first high element with pivot */
	return (i + 1);
}

void quick_sort(int *m, int l, int h)
{
	if (l < h) {
		int p = partition(m, l, h);

		quick_sort(m, l, p - 1);
		quick_sort(m, p + 1, h);
	}
}

int main(int argc, char **argv)
{
	int array[] = {6, 2, 8, 3, 9, 4, 0, 3, 1, 6};
	size_t array_sz = sizeof(array)/sizeof(int);
	int i;

	puts("Before:");
	for (i = 0; i < array_sz; i++)
		printf("%d ", array[i]);

	puts("\nAfter:");
	quick_sort(array, 0, array_sz - 1);
	for (i = 0; i < array_sz; i++)
		printf("%d ", array[i]);
	puts("");

	return 0;
}
