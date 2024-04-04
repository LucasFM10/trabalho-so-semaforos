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

// semáforo para garantir que todos os consumidores leiam uma mensagem por vez.
// o último consumidor libera os outros dando post em semTodosConsumidoresLeram,
// indicando que todos os consumidores já consumiram a mensagem.
// após isso, o útlimo consumidor dá 3 waits em semUltimoConsumidorEsperando,
// que será liberado pelos outros consumidores após terem esperando todos
// serem lidos. isso garante que, quando o último consumidor liberar 3 vezes
// semEsperandoUltimoConsumidor e todos os outros consumidores serem liberados, isso garante
// todos os consumidores irão consumir 1 vez aquela mensagem. tentei fazer sem essa lógica a princípio,
// mas sem esses bloqueios poderia acontecer de um consumidor ler a mensagem 3 vezes e 2 consumidores não lerem, por exemplo
sem_t semTodosConsumidoresLeram;
sem_t semUltimoConsumidorEsperando;
sem_t semEsperandoUltimoConsumidor;

sem_t controlar_produc;

pthread_mutex_t mutexBuffer;

std::vector<std::string> buffer(BUFFER_SIZE); // buffer para armazenar mensagens
std::vector<int> index_producs(THREAD_NUM / 2);

int in = 0; // Indica onde o próximo produtor deve escrever
int consumedCount = 0; // Contador de quantos consumidores já consumiram uma mensagem


// Função que coloca o produtor que terminou de enviar a mensagem na vez de ser consumido.
void trocaComPrimeiro(std::vector<std::string>& vetor, int producer_id) {
    
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

    // Adiciona ao buffer
    sem_wait(&semEmpty);

    int producer_id = *((int*)args); // Extrai o ID do produtor
    std::string message = "";
    pthread_mutex_lock(&mutexBuffer);
    index_producs[producer_id] = in;
    in++; // Avança para o próximo local no buffer, considerando o tamanho do buffer
    pthread_mutex_unlock(&mutexBuffer);

    for (int i = 0; i < WORDS_PRODUCER; i++) {
        // Produz
        int x = rand() % 100;
        std::string xComoString = std::to_string(x) + " ";

        message.append(xComoString);
        
        sleep(2);
        pthread_mutex_lock(&mutexBuffer);
        buffer[index_producs[producer_id]].append(xComoString);
        // for (std::size_t i = 0; i < buffer.size(); ++i) {
        //     if (!buffer[i].empty()) {
        //         std::cout << "Índice " << i << ": " << buffer[i] << std::endl;
        //     }
        // }
        // std::cout << std::endl;
        pthread_mutex_unlock(&mutexBuffer);
    }

    pthread_mutex_lock(&mutexBuffer);
    std::cout << "Producer "<< producer_id << " produced: " << message << " -----\n"; // Imprime o ID do produtor e o valor produzido 
    pthread_mutex_unlock(&mutexBuffer);

    sem_wait(&controlar_produc);
    pthread_mutex_lock(&mutexBuffer);
    trocaComPrimeiro(buffer, producer_id);
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
    
            sem_post(&controlar_produc);
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
    sem_init(&controlar_produc, 0, 1);
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
    sem_destroy(&controlar_produc);
    pthread_mutex_destroy(&mutexBuffer);
    return 0;
}
