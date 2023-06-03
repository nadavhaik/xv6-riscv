
int main()
{
    int size = 1;
    for(int i=0; i<15; i++)
    {
        printf("mallocing %d\n", size);
        malloc(size);
        size *= 2;
    }

    return 0;
}