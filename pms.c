#include "pms.h"
#define DEBUG 1

int main(int argc, char *argv[]){

    int numprocs;               /* pocet procesorov */
    int myid;                   /* id procesora */
    MPI_Status stat;            /* struct- obsahuje kod- source, tag, error */
    FILE *handler;              /* deskr. pre subor s cislami */
    int *numbers;               /* vsetky nacitane cisla zo suboru */
    int number;                 /* nacitane cislo */
    int numbers_count;          /* pocet nacitanych cisiel */
    int **up,**down;            /* fronty */
    int count_up = 0;           /* pocet cisiel prijatych na hornu frontu */
    int count_down = 0;         /* pocet cisiel prijatych na dolnu frontu */
    int opt;                    /* parameter z prikazovej riadky */
    int i,j = 0;                /* pomocne premenne */
    int flag_read = 0;          /* ak je 1, tak je precitany cely subor */
    int changed_up = 0;         /* nastavene na 1 ak sa odoslala up hodnota */
    int changed_down = 0;       /* nastavene na 1 ak sa odoslala down hodnota */

    /* MPI INIT */
    MPI_Init(&argc,&argv);                          /* inicializacia MPI */
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);       /* zisti kolko procesov bezi */
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);           /* zisti id svojho procesu */

    /* alokacia pamate pre cisla na vstupe */
    numbers = (int *)malloc(INIT_NUMBERS_SIZE * sizeof(int));

    /* alokacia pamete pre vsetky procesory */
    up = (int *) malloc(numprocs * sizeof(int));
    down = (int *) malloc(numprocs * sizeof(int));


    /*
     * alokacia pamate vo frontach pre cisla, zacina na druhom procesore
     * s hodnotou pre dve cisla pre kazdu frontu
     */
    for(i = 1; i < numprocs; i++){
            up[i] = (int *) malloc((i + 1) * sizeof(int));
            down[i] = (int *) malloc((i + 1) * sizeof(int));
    }

    /* z paremetru ziskaj pocet spracovavanych cisiel */
    while((opt = getopt(argc, argv, "n:")) != -1) {
            if(opt == 'n'){
                numbers_count = atoi(optarg);
            }
    }

    /* udava pocet cyklov po ktorych musi byt zoradene */
    int cycle_end = (numbers_count - 1) + pow(2,numprocs - 1) + (numprocs - 1);

    /* prvy procesor si otvori subor na citanie */
    if(myid == 0){
        if((handler = fopen(FILENAME,"r")) == NULL){
            fprintf(stderr,"Open file failed!" );
            exit(EXIT_FAILURE);
         }
    }

    for(i = 0; i < cycle_end; i++){

        /* prvy procesor cita zo suboru cisla */
        if(myid == 0){

            if(flag_read == 0){

                /* postupne citaj subor, az kym nenarazi na koniec */
                if((number = getc(handler)) != EOF){

                    /* ak je treba alokuj viac pamate */
                    if(i % INIT_NUMBERS_SIZE == 0 && i != 0){
                        numbers = realloc(numbers,(i + INIT_NUMBERS_SIZE) * sizeof(int));
                    }

                    /* postupne posielaj na frontu nasledujucemu procesoru */
                    MPI_Send(&number, 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                    numbers[i] = number;
                }else{
                    #ifdef DEBUG
                    printf("koncim s citanim subora v cykle: %d\n",i+1);
                    #endif
                    flag_read = 1;
                }
            }

            /* na konci citania vypis cisla a uzatvor subor */
            if(i == numbers_count){
                print_numbers(numbers,numbers_count);
                fclose(handler);
            }

        }else{/* ostane proc. prijimaju/odosielaju cisla a porovnavaju ich */

            MPI_Recv(&number, 1, MPI_INT, (myid - 1), TAG, MPI_COMM_WORLD, &stat);


            /* ak sa v predchadajucom kroku zamenili hodnoty tak len posli na
             * nasledujuci procosor zostavajucu hodnotu
             */
            if(changed_up == 1){
                #ifdef DEBUG
                printf("%d - posielam zostavajucu down hodnotu %i\n",
                       myid+1,down[myid][count_up - 1]);
                #endif

                /* bude sa posielat zostavajuca down hodnota */
                MPI_Send(&down[myid][count_up - 1], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                count_down--;
                changed_up = 0;

            }else if(changed_down == 1){

                #ifdef DEBUG
                printf("%d - posielam zostavajucu up hodnotu %i\n",
                       myid+1,up[myid][count_down - 1]);
                #endif

                /* bude sa posielat zostavajuca down hodnota */
                MPI_Send(&up[myid][count_down - 1], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);

                count_up--;
                changed_down = 0;

            }else{

                /* velkost fronty, ktora udava kedy ma dany procesor zacat porovanvat */
                int q_size = pow(2,((myid + 1) - 2));

                #ifdef DEBUG
                //printf("%d - velkost %i v cykle %d s cislom %d\n",myid,q_size,i,number);
                //printf("%d - porovnavam size:%i == count_up:%i\n",myid+1,q_size,count_up);
                //printf("%d - porovnavam size:%i == count_down:%i\n",myid+1,q_size,count_down);
                #endif

                /* velkost jednej fronty dosiahla pozadovanu velkost a na druhej je aspon
                 * jedna hodnota
                 */
                if(q_size == count_up && count_down >= 1){
                    #ifdef DEBUG
                    printf("UP - procesor c. %d zacal porovnavat v kroku %d cisla %i a %i\n",
                           myid + 1,i + 1,up[myid][count_down - 1],down[myid][count_down - 1]);
                    #endif

                    /* mensie cislo posli prve dalsiemu procesoru */
                    if(up[myid][count_down - 1] < down[myid][count_down - 1]){
                        /* dalsiemu procesoru posli cislo z up */
                        #ifdef DEBUG
                        printf("%d - na %i. procesor posielam cislo %i\n ",myid+1,myid+2,up[myid][count_down - 1]);
                        #endif
                        MPI_Send(&up[myid][count_down - 1], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                        /* na miesto up cisla mozes vlozit nove prichadzajuce */
                        count_up--;
                        changed_up = 1;
                    }else if(up[myid][count_down - 1] > down[myid][count_down - 1]){
                        /* dalsiemu procesoru posli cislo z down */
                        #ifdef DEBUG
                        printf("%d - na %i. procesor posielam cislo %i\n ",myid+1,myid+2,down[myid][count_up - 1]);
                        #endif
                        MPI_Send(&down[myid][count_up - 1], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                        /* na miesto up cisla mozes vlozit nove prichadzajuce */
                        count_down--;
                        changed_down = 1;
                    }


                }else if(q_size == count_down && count_up >= 1){
                    #ifdef DEBUG
                    printf("DOWN - procesor c. %d zacal porovnavat v kroku %d cisla %i a %i\n",
                           myid + 1,i + 1,down[myid][count_up - 1],up[myid][count_up - 1]);
                    #endif

                                        /* mensie cislo posli prve dalsiemu procesoru */
                    if(down[myid][count_up - 1] < up[myid][count_up - 1]){
                        /* dalsiemu procesoru posli cislo z up */
                        #ifdef DEBUG
                        printf("%d - na %i. procesor posielam cislo %i\n ",myid+1,myid+2,down[myid][count_up - 1]);
                        #endif
                        MPI_Send(&down[myid][count_up - 1], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                        /* na miesto up cisla mozes vlozit nove prichadzajuce */
                        count_down--;
                        changed_down = 1;
                    }else if(down[myid][count_up - 1] > up[myid][count_up - 1]){
                        /* dalsiemu procesoru posli cislo z down */
                        #ifdef DEBUG
                        printf("%d - na %i. procesor posielam cislo %i\n ",myid+1,myid+2,up[myid][count_down - 1]);
                        #endif
                        MPI_Send(&up[myid][count_down - 1], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                        /* na miesto up cisla mozes vlozit nove prichadzajuce */
                        count_up--;
                        changed_up = 1;
                    }
                }
            }

                /* striedavo ukladaj na fronty
                 * pricom kazda fronta ma veklost myid + 1, takze druhy procesor
                 * ma velkost front 2, treti 3...atd.
                 */
                if(i % (myid + 1) == 0){
                    #ifdef DEBUG
                    printf("%d - do up %i na poziciu %d\n",myid+1,number,count_up);
                    #endif
                    up[myid][count_up++] = number;
                }else{
                    #ifdef DEBUG
                    printf("%d - do down %i na poziciu %d\n",myid+1,number,count_down);
                    #endif
                    down[myid][count_down++] = number;
                }
        }

        //printf("%d - koncim cyklus %d\n",myid, i);
    }

    MPI_Finalize();
    return 0;
}

void print_numbers(int *numbers,int length){
    int i;

    for(i = 0; i < length; i++){
        printf("%i ",numbers[i]);
    }
    printf("\n");
}
