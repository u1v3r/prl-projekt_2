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
    int recv = 0;

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
            up[i] = (int *) malloc(numprocs * sizeof(int));
            down[i] = (int *) malloc(numprocs * sizeof(int));
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

    /* urcuje index pola porovnavanych hodnot */
    int compare_index = 0;
    int index_up = 0;
    int index_down = 0;
    int save_up = 1;
    int k = 0;
    int q_size = 0;

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


            /* ak prijal vsetky cisla tak uz necakaj na dalsie */
            if(recv != numbers_count){
                MPI_Recv(&number, 1, MPI_INT, (myid - 1), TAG, MPI_COMM_WORLD, &stat);
                recv++;
            }

            //printf("%d - dosla hodnota %i\n",myid,number);


            /* posledny procesor uz dalej neposiala, ale len vysuva cisla */
            #ifdef DEBUG
            if(myid == (numprocs - 1)){
                printf("\n\n%d - som posledny a prijal som cislo %i\n\n",myid+1,number);
            }
            #endif

            /* ak sa v predchadajucom kroku zamenili hodnoty tak len posli na
             * nasledujuci procosor zostavajucu hodnotu
             */
            if(changed_up == 1){
                #ifdef DEBUG
                printf("%d - posielam zostavajucu down hodnotu %i na indexe %i\n",
                       myid+1,down[myid][compare_index],compare_index);
                #endif


                /* bude sa posielat zostavajuca down hodnota */
                MPI_Send(&down[myid][compare_index], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                count_down--;
                compare_index++;
                changed_up = 0;

            }else if(changed_down == 1){

                #ifdef DEBUG
                printf("%d - posielam zostavajucu up hodnotu %i na indexe %i\n",
                       myid+1,up[myid][compare_index],compare_index);
                #endif

                /* bude sa posielat zostavajuca down hodnota */
                MPI_Send(&up[myid][compare_index], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);

                compare_index++;
                count_up--;
                changed_down = 0;

            }else{

                /* velkost fronty, ktora udava kedy ma dany procesor zacat porovanvat */
                q_size = pow(2,((myid + 1) - 2));

                #ifdef DEBUG
                //printf("%d - velkost %i v cykle %d s cislom %d\n",myid,q_size,i,number);
                //printf("%d - porovnavam size:%i == count_up:%i\n",myid+1,q_size,count_up);
                //printf("%d - porovnavam size:%i == count_down:%i\n",myid+1,q_size,count_down);
                #endif

                /* velkost jednej fronty dosiahla pozadovanu velkost a na druhej je aspon
                 * jedna hodnota
                 */
                if(q_size == count_up && count_down == 1){
                    #ifdef DEBUG
                    printf("UP - procesor c. %d zacal porovnavat v kroku %d cisla %i a %i\n",
                           myid + 1,i + 1,up[myid][compare_index],down[myid][compare_index]);
                    #endif

                    /* mensie cislo posli prve dalsiemu procesoru */
                    if(up[myid][compare_index] < down[myid][compare_index]){
                        /* dalsiemu procesoru posli cislo z up */
                        #ifdef DEBUG
                        printf("%d - na %i. procesor posielam cislo %i\n",myid+1,myid+2,up[myid][compare_index]);
                        #endif


                        /* posledny procesor len vysuva, nic neposiela */
                        if(myid == (numprocs - 1)){

                        }else{

                            MPI_Send(&up[myid][compare_index], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                            /* na miesto up cisla mozes vlozit nove prichadzajuce */
                            count_up--;
                            changed_up = 1;
                        }



                    }else if(up[myid][compare_index] > down[myid][compare_index]){
                        /* dalsiemu procesoru posli cislo z down */
                        #ifdef DEBUG
                        printf("%d - na %i. procesor posielam cislo %i\n",myid+1,myid+2,down[myid][compare_index]);
                        #endif
                        /* posledny procesor len vysuva, nic neposiela */
                        if(myid == (numprocs - 1)){

                        }else{
                            MPI_Send(&down[myid][compare_index], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                            /* na miesto up cisla mozes vlozit nove prichadzajuce */
                            count_down--;
                            changed_down = 1;
                        }
                    }


                }else if(q_size == count_down && count_up == 1){
                    #ifdef DEBUG
                    printf("DOWN - procesor c. %d zacal porovnavat v kroku %d cisla %i a %i\n",
                           myid + 1,i + 1,down[myid][compare_index],up[myid][compare_index]);
                    #endif

                                        /* mensie cislo posli prve dalsiemu procesoru */
                    if(down[myid][compare_index] < up[myid][compare_index]){
                        /* dalsiemu procesoru posli cislo z up */
                        #ifdef DEBUG
                        printf("%d - na %i. procesor posielam cislo %i\n ",myid+1,myid+2,down[myid][compare_index]);
                        #endif
                        /* posledny procesor len vysuva, nic neposiela */
                        if(myid == (numprocs - 1)){

                        }else{
                            MPI_Send(&down[myid][compare_index], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                            /* na miesto up cisla mozes vlozit nove prichadzajuce */
                            count_down--;
                            changed_down = 1;
                        }
                    }else if(down[myid][compare_index] > up[myid][compare_index]){
                        /* dalsiemu procesoru posli cislo z down */
                        #ifdef DEBUG
                        printf("%d - na %i. procesor posielam cislo %i\n",myid+1,myid+2,up[myid][compare_index]);
                        #endif
                        /* posledny procesor len vysuva, nic neposiela */
                        if(myid == (numprocs - 1)){

                        }else{
                            MPI_Send(&up[myid][compare_index], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                            /* na miesto up cisla mozes vlozit nove prichadzajuce */
                            count_up--;
                            changed_up = 1;
                        }
                    }
                }
            }



                /* striedavo ukladaj na fronty pricom kazda fronta ma veklost
                 * 2^i-2
                 */
                if(pow(2,(myid+1)-2) == k){
                    if(save_up == 0){
                        save_up = 1;
                    }else {
                        save_up = 0;
                    }

                    k = 0;
                }
                k++;
                if(save_up == 1){
                    #ifdef DEBUG
                    printf("%d - do up %i na poziciu %d\n",myid+1,number,index_up);
                    #endif
                    up[myid][index_up++] = number;
                    count_up++;
                }else{
                    #ifdef DEBUG
                    printf("%d - do down %i na poziciu %d\n",myid+1,number,index_down);
                    #endif
                    down[myid][index_down++] = number;
                    count_down++;
                }

        }

    }

    #ifdef DEBUG
    printf("\n\n\nKoniec %d\n\n\n",myid+1);
    #endif

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
