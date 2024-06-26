#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>

#define THREAD_NUM 8 // metade das threads vai ser produtora e a outra metade será consumidora
#define BUFFER_SIZE 100 // tamanho do buffer, pode armazenar até 100 strings, tamanho mais do que suficiente
#define WORDS_PRODUCER 3 // número de palavras que cada produtor irá enviar

sem_t semEmpty; // semáforo para impedir que os produtores tentem popular um buffer cheio
sem_t semFull; // semáforo para impedir que os consumidores tentem consumir o buffer quando ele estiver vazio

// Estes semáforos garantem que todos os consumidores leiam uma mensagem por vez.
// O último consumidor libera os outros dando post em semTodosConsumidoresLeram,
// indicando que todos os consumidores já consumiram a mensagem.
// Após isso, o útlimo consumidor dá um número waits equivalente ao número de consumidores
// em semUltimoConsumidorEsperando.
// Então, os outros consumidores liberarão o último consumidor dando post em semUltimoConsumidorEsperando.
// Isso garante que cada consumidor irá consumir cada mensagem apenas uma vez, e
// que todos os consumidores irão consumir a mensagem.
sem_t semTodosConsumidoresLeram;
sem_t semUltimoConsumidorEsperando;
sem_t semEsperandoUltimoConsumidor;

// Semáforo para garantir que sempre que um produtor terminar de
// enviar a sua mensagem ela seja consumida e nenhum outro produtor ocupe sua vez.
// Além disso, também permite, em conjunto com os mutexes, que a produção
// possa se dar simultaneamente ao consumo.
sem_t semControlaProduc;

pthread_mutex_t mutexBuffer;

std::vector<std::string> buffer(BUFFER_SIZE); // buffer para armazenar mensagens
std::vector<int> index_producs(THREAD_NUM / 2);

int in = 0; // Indica onde o próximo produtor deve escrever
int consumedCount = 0; // Contador de quantos consumidores já consumiram uma mensagem


// Função que coloca o produtor que terminou de enviar a mensagem na vez de ser consumido.
void moveParaPrimeiraPosicao(std::vector<std::string>& vetor, int producer_id) {
    
    int index = index_producs[producer_id];
    for (std::size_t i = 0; i < index_producs.size(); ++i) {
        if (index_producs[i] == 0) {
            index_producs[i] = index;
            index_producs[producer_id] = 0;
            break; // Sai do loop uma vez que o valor é encontrado
        }
    }
    if (index < vetor.size()) {
        std::swap(vetor[0], vetor[index]);
    } else {
        std::cout << "Índice fornecido é maior que o tamanho do vetor." << std::endl;
    }
}

void* producer(void* args) {
    // Inicializa o gerador de números aleatórios
    srand(static_cast<unsigned int>(time(0) + *((int*)args))); // Adiciona o ID do produtor para diversificar a semente


    // Adiciona ao buffer
    sem_wait(&semEmpty);

    int producer_id = *((int*)args); // Extrai o ID do produtor
    pthread_mutex_lock(&mutexBuffer);
    index_producs[producer_id] = in;
    in++; // Avança para o próximo local no buffer, considerando o tamanho do buffer
    pthread_mutex_unlock(&mutexBuffer);

    // Número de palavras para a mensagem (caso queira que seja aleatório)
    int nPalavras = rand() % 4 + 4;

    for (int i = 0; i < WORDS_PRODUCER; i++) {

        // Determina um tamanho aleatório para a string entre 3 e 7
        int tamanhoString = rand() % 5 + 3; // rand() % 5 gera um valor de 0 a 4, adicionando 3 resulta em um valor de 3 a 7

        std::string resultado;

        for (int i = 0; i < tamanhoString; ++i) {
            // Gera um número aleatório entre 0 e 25
            char letra = 'a' + (rand() % 26);
            // Adiciona a letra à string
            resultado += letra;
        }
        
        sleep(2);
        pthread_mutex_lock(&mutexBuffer);
        buffer[index_producs[producer_id]].append(resultado + " ");
        std::cout << "Producer " << producer_id << " produziu a palavra " << resultado << " e agora o buffer está assim:\n";
        for (std::size_t i = 0; i < buffer.size(); ++i) {
            if (!buffer[i].empty()) {
                std::cout << "Índice " << i << ": " << buffer[i] << std::endl;
            }
        }
        std::cout << std::endl;
        pthread_mutex_unlock(&mutexBuffer);
    }

    pthread_mutex_lock(&mutexBuffer);
    std::cout << "Producer "<< producer_id << " produced: " << buffer[index_producs[producer_id]] << " -----\n\n"; // Imprime o ID do produtor e o valor produzido 
    pthread_mutex_unlock(&mutexBuffer);

    sem_wait(&semControlaProduc);
    pthread_mutex_lock(&mutexBuffer);
    moveParaPrimeiraPosicao(buffer, producer_id);
    pthread_mutex_unlock(&mutexBuffer);

    for (int i = 0; i < THREAD_NUM / 2; i++)
        sem_post(&semFull);
    
    return NULL;
}

void* consumer(void* args) {
    int consumer_id = *((int*)args); // Extrai o ID do consumidor
    while (1) {

        std::string y;

        sem_wait(&semFull);


        pthread_mutex_lock(&mutexBuffer);
        y = buffer[0];// Consome
        printf("Consumer %d consumed: %s\n", consumer_id, y.c_str()); // Imprime o ID do consumidor e o valor consumido
        consumedCount++;


        if (consumedCount == (THREAD_NUM / 2)) {

            // Remove do buffer após todos os consumidores lerem a palavra
            consumedCount = 0;
            buffer.erase(buffer.begin());;
            for (int i = 0; i < index_producs.size(); i++) {
                index_producs[i] -= 1;
            }
            in--;
            sem_post(&semEmpty);
            pthread_mutex_unlock(&mutexBuffer);

            for (int i = 0; i < (THREAD_NUM / 2) - 1; i++) {
                sem_post(&semTodosConsumidoresLeram);
            }

            for (int i = 0; i < (THREAD_NUM / 2) - 1; i++) {
                sem_wait(&semUltimoConsumidorEsperando);
            }

            for (int i = 0; i < (THREAD_NUM / 2) - 1; i++) {
                sem_post(&semEsperandoUltimoConsumidor);
            }
    
            sem_post(&semControlaProduc);
        } else {
            pthread_mutex_unlock(&mutexBuffer);
            sem_wait(&semTodosConsumidoresLeram);
            sem_post(&semUltimoConsumidorEsperando);
            sem_wait(&semEsperandoUltimoConsumidor);
        }


        
        

    }
    return NULL;
}


int main(int argc, char* argv[]) {
    srand(time(NULL));
    pthread_t th[THREAD_NUM];
    pthread_mutex_init(&mutexBuffer, NULL);
    sem_init(&semEmpty, 0, BUFFER_SIZE);
    sem_init(&semFull, 0, 0);
    sem_init(&semTodosConsumidoresLeram, 0, 0);
    sem_init(&semEsperandoUltimoConsumidor, 0, 0);
    sem_init(&semUltimoConsumidorEsperando, 0, 0);
    sem_init(&semControlaProduc, 0, 1);
    int i;
    int ids[THREAD_NUM]; // Array para armazenar IDs das threads
    for (i = 0; i < THREAD_NUM; i++) {
        ids[i] = i / 2; // Atribui IDs únicos a cada thread
        if (i % 2 == 0) {
            if (pthread_create(&th[i], NULL, &producer, (void*)&ids[i]) != 0) {
                perror("Failed to create thread");
            }
        } else {
            if (pthread_create(&th[i], NULL, &consumer, (void*)&ids[i]) != 0) {
                perror("Failed to create thread");
            }
        }
    }
    for (i = 0; i < THREAD_NUM; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join thread");
        }
    }
    sem_destroy(&semEmpty);
    sem_destroy(&semFull);
    sem_destroy(&semTodosConsumidoresLeram);
    sem_destroy(&semEsperandoUltimoConsumidor);
    sem_destroy(&semUltimoConsumidorEsperando);
    sem_destroy(&semControlaProduc);
    pthread_mutex_destroy(&mutexBuffer);
    return 0;
}
