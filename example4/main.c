/*
 * main.c
 *
 *  Created on: Nov 9, 2025
 *      Author: acer
 */


#include<stdio.h>
int m = 10;
int main()
{

	long long int n = (long long int)&m;
	printf("%p", &m);
	return 0;
}
