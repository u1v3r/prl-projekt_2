#include "pms.h"
//#define DEBUG 1

int main(int argc, char *argv[]){

    int numprocs;               /* pocet procesorov */
    int myid;                   /* id procesora */
    MPI_Status stat;            /* struct- obsahuje kod- source, tag, error */
    FILE *handler;              /* handler pre subor s cislami */
    int *numbers;               /* vsetky nacitane cisla zo suboru */
    int number;                 /* nacitane cislo */
    int numbers_count;          /* pocet nacitanych cisiel */
    int **up,**down;            /* fronty */
    int count_up = 0;           /* pocet cisiel prijatych na hornu frontu */
    int count_down = 0;         /* pocet cisiel prijatych na dolnu frontu */
    int opt;                    /* parameter z prikazovej riadky */
    int i,j = 0;                /* pomocne premenne */
    int flag_read = 0;          /* ak je 1, tak je precitany cely subor */
    int flag_end = 0;           /* ak je 1, tak su prijate vsetky cisla */
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
    int compare_up = 0;
    int compare_down = 0;
    int flag_start = 0;
    int flag_change = 0;
    int counter = 0;
    int flag_shift_up = 0;
    int flag_shift_down = 0;

    for(i = 0; i < 25; i++){

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


            //if(flag_end == 1) break;

            /* ak prijal vsetky cisla tak uz necakaj na dalsie a ak je koniec nastav flag_end */
            if(recv != numbers_count){
                MPI_Recv(&number, 1, MPI_INT, (myid - 1), TAG, MPI_COMM_WORLD, &stat);
                /*if(myid == 2){
                    printf("prijal som %d\n",number);
                }*/
                recv++;
            }else {
                flag_end = 1;
            }


            /* na druhom procesore sa prepina flag po jednom porovnani
             * na tretom po dvoch porovnaniach...atd.
             */
            int power = pow(2,myid) - 1;
            //printf("%i - conter = %i, pow = %i\n",myid+1,counter,power);
            if(counter == power){
                #ifdef DEBUG
                printf("%i - menim flag na hodnote %i\n",myid + 1, power);
                #endif
                flag_change = 1;
                counter = 0;
            }



            //printf("\n\n%d - sum:%d == myid:%d\n\n",myid+1,changed_up+changed_down,myid);


                q_size = pow(2,((myid + 1) - 2));
                /* velkost fronty, ktora udava kedy ma dany procesor zacat porovanvat */
                if(q_size == count_up) flag_start = 1;

                #ifdef DEBUG
                //printf("%d - velkost %i v cykle %d s cislom %d\n",myid,q_size,i,number);
                //printf("%d - porovnavam size:%i == count_up:%i\n",myid+1,pow(2,((myid + 1) - 2)),count_up);
                //printf("%d - porovnavam size:%i == count_down:%i\n",myid+1,pow(2,((myid + 1) - 2)),count_down);
                #endif

                /* velkost hornej fronty dosiahla pozadovanu velkost a na dolnej je aspon
                 * jedna hodnota
                 */
                if(flag_start == 1 && count_down >= 1){

                        /* postupnost bola zoradena nastav indexy compare_down a compare_up */
                        if(flag_change == 1){

                            if(changed_up == 1) {
                                #ifdef DEBUG
                                printf("%d - posielam zostavajucu down hodnotu %i na indexe %i\n",
                                       myid+1,down[myid][compare_down],compare_down);
                                #endif
                                if(myid == (numprocs - 1)){
                                    printf("1 - %d\n",down[myid][compare_down]);
                                }else {
                                    MPI_Send(&down[myid][compare_down], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                                }
                                compare_down = compare_up;
                                changed_up = 0;
                            }

                            if(changed_down == 1){
                                #ifdef DEBUG
                                printf("%d - posielam zostavajucu up hodnotu %i na indexe %i\n",
                                       myid+1,up[myid][compare_up],compare_up);
                                #endif
                                if(myid == (numprocs - 1)){
                                    printf("2 - %d\n",up[myid][compare_up]);
                                } else {
                                    MPI_Send(&up[myid][compare_up], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                                }
                                compare_up = compare_down;
                                changed_down = 0;
                            }

                            flag_change = 0;

                        }else {

                            if((compare_up - compare_down == 0) && flag_shift_down == 1 ) {
                                flag_shift_down = 0;
                            }
                            if((compare_down - compare_up == 0) && flag_shift_up == 1) {
                                flag_shift_up = 0;
                            }

                            /* ide na index, ktory je za hranicou, tak len vysun hodnoty */
                            if( (compare_up - compare_down == q_size) || flag_shift_down == 1){
                                #ifdef DEBUG
                                printf("%i - Vysuvam down hodnotu %i\n",myid+1,down[myid][compare_down]);
                                #endif

                                if(myid == (numprocs - 1)){
                                    printf("3 - %d\n",down[myid][compare_down]);
                                } else {
                                    MPI_Send(&down[myid][compare_down], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                                }

                                flag_shift_down = 1;
                                compare_down++;
                                counter++;
                            } else if((compare_down - compare_up == q_size) || flag_shift_up == 1){
                                #ifdef DEBUG
                                printf("%i - Vysuvam up hodnotu %i\n",myid+1,up[myid][compare_up]);
                                #endif
                                if(myid == (numprocs - 1)){
                                    printf("4 - %d\n",up[myid][compare_up]);
                                }else {
                                    MPI_Send(&up[myid][compare_up], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                                }
                                flag_shift_up = 1;
                                compare_up++;
                                counter++;
                            }else{

                                #ifdef DEBUG
                                printf("UP - procesor c. %d zacal porovnavat v kroku %d cisla %i:%i a %i:%i\n",
                                       myid + 1,i + 1,compare_up,up[myid][compare_up],compare_down,down[myid][compare_down]);
                                #endif

                                /* mensie cislo posli prve dalsiemu procesoru */
                                if(up[myid][compare_up] <= down[myid][compare_down]){


                                        /* dalsiemu procesoru posli cislo z up */
                                        #ifdef DEBUG
                                        printf("%d - na %i. procesor posielam cislo %i\n",myid+1,myid+2,up[myid][compare_up]);
                                        #endif
                                        if(myid == (numprocs - 1)){
                                            printf("5 - %d\n",up[myid][compare_up]);
                                        }
                                        else {
                                            MPI_Send(&up[myid][compare_up], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                                        }

                                        /* pocet cisel na up zasobniku sa znizil o 1 */
                                        count_up--;

                                        counter++;

                                        /* v nasledujucom kroku bude porovnavat s dalsim up v poradi */
                                        compare_up++;

                                        changed_up = 1;
                                        changed_down = 0;


                                }else if(up[myid][compare_up] > down[myid][compare_down]){


                                        /* dalsiemu procesoru posli cislo z down */
                                        #ifdef DEBUG
                                        printf("%d - na %i. procesor posielam cislo %i\n",myid+1,myid+2,down[myid][compare_down]);
                                        #endif
                                        if(myid == (numprocs - 1)){
                                            printf("6 - %d\n",down[myid][compare_down]);
                                        }
                                        else {
                                            MPI_Send(&down[myid][compare_down], 1, MPI_INT, (myid + 1), TAG, MPI_COMM_WORLD);
                                        }


                                        /* pocet cisel na down zasobniku sa znizil o 1 */
                                        count_down--;

                                        changed_down = 1;
                                        changed_up = 0;

                                        counter++;

                                        /* v nasledujucom kroku bude porovnavat s dalsim down v poradi */
                                        compare_down++;
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

            if(flag_end == 0){
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
            k++;
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
