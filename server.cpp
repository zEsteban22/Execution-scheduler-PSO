#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <sstream>

using namespace std;
using chrono::duration_cast;
using chrono::milliseconds;
static std::chrono::time_point<std::chrono::system_clock> now() { return std::chrono::system_clock::now(); };

typedef std::chrono::high_resolution_clock Time;

#define MAX 500
#define port 5200

struct Proceso
{
	int burst;
	int prioridad;
	int pid;
	bool operator==(const Proceso otro)
	{
		return pid == otro.pid;
	}
};
struct pcb
{
	int pid;
	int estado;
	time_t wt;
	time_t tat;
};

vector<Proceso> ready;
vector<pcb> listPCB;
pthread_mutex_t readyLock;
mutex ociosidad;
atomic<bool> corriendo(true);
atomic<int> pid(0);
atomic<long> tiempoOcioso(0);

void *esperaMensaje(void *con)
{
	int connection = *((int *)(&con));
	char buff[MAX] = "";
	while (true)
	{
		int rMsgSize;
		if ((rMsgSize = recv(connection, buff, MAX, 0)) > 0)
		{
			cout << "Client: " << buff << endl;
			if (buff[0] == 'b' && buff[1] == 'y' && buff[2] == 'e')
			{
				cout << "\nSe terminó la conexión, enviando resumen al cliente." << endl;
				stringstream ss;
				ss << "Estado de la cola de ready!" << endl;
				ss << "Largo = " << ready.size() << endl;
				ss << "Elementos:" << endl;
				Proceso p;
				for (int i = 0; i < ready.size(); i++)
				{
					p = ready.at(i);
					ss << "PID " << p.pid << ": " << p.burst << ',' << p.prioridad << endl;
				}
				ss << "Y estuvo " << tiempoOcioso << " milisegundos haciendo nada.\n"
				   << endl;

				pcb pcb1;
				ss << "Tabla de TAT y WT" << endl;
				int cantidad;
				float sumaWT;
				float sumaTAT;
				for (int i = 0; i < listPCB.size(); i++)
				{
					pcb1 = listPCB.at(i);
					if (pcb1.estado == 1)
					{
						cantidad += 1;
						sumaTAT += pcb1.tat;
						sumaWT += pcb1.wt;
						ss << "Process ID: " << pcb1.pid << endl;
						ss << "WT: " << pcb1.wt << "\nTAT: " << pcb1.tat << "\n"
						   << endl;
					}
				}
				sumaWT = sumaWT / cantidad;
				sumaTAT = sumaTAT / cantidad;
				ss << "Promedio de TAT: " << sumaTAT << "\nPromedio WT: " << sumaWT;
				int l = ss.str().length();
				for (int i = 0; i < l; i++)
					buff[i] = ss.get();
				send(connection, buff, l + 1, 0);
				corriendo = false;
				pthread_exit(NULL);
				break;
			}
			else if (buff[0] == 'c' && buff[1] == 'o' && buff[2] == 'l' && buff[3] == 'a')
			{
				stringstream ss;
				ss << "Estado de la cola de ready!" << endl;
				ss << "Largo = " << ready.size() << endl;
				ss << "Elementos:" << endl;
				for (auto p : ready)
					ss << "PID " << p.burst << ": " << p.burst << ',' << p.prioridad << endl;
				int l = ss.str().length();
				for (int i = 0; i < l; i++)
					buff[i] = ss.get();
				send(connection, buff, l + 1, 0);
			}
			else
			{
				string linea = buff;
				pthread_mutex_lock(&readyLock);
				ready.push_back(
					{atoi(linea.substr(0, linea.find(',')).c_str()),
					 atoi(linea.substr(linea.find(',') + 1, linea.length()).c_str()),
					 pid++});
				time_t timer = time(NULL);
				listPCB.push_back({pid - 1, 0, timer, timer});
				if (ready.size() == 1)
					ociosidad.unlock();
				pthread_mutex_unlock(&readyLock);
				linea = "pid = " + to_string(pid - 1) + ".\n";
				char buff1[MAX];
				int l = linea.length();
				for (int i = 0; i < l; i++)
					buff1[i] = linea.at(i);
				send(connection, buff1, l, 0);
			}
		}
	}
	pthread_exit(NULL);
}

void *algoritmoFIFO(void *)
{
	ociosidad.lock();
	ociosidad.lock();
	while (corriendo.load())
	{
		Proceso p;
		bool flag = false;
		pthread_mutex_lock(&readyLock);
		if (ready.size() > 0)
		{
			p = ready.at(0);
			ready.erase(ready.begin());
			flag = true;
		}
		pthread_mutex_unlock(&readyLock);
		if (flag)
		{
			cout << "Ejecutando el proceso con pid = "
				 << p.pid << " por " << p.burst << " segundos." << endl;
			listPCB.at(p.pid).wt = time(NULL) - listPCB.at(p.pid).wt;
			sleep(p.burst);
			listPCB.at(p.pid).tat = time(NULL) - listPCB.at(p.pid).tat;
			listPCB.at(p.pid).estado = 1;
		}
		else
		{
			cout << "El procesador está ocioso, iniciando cronómetro de tiempo ocioso." << endl;
			const auto estampa = now();
			ociosidad.lock();
			tiempoOcioso += duration_cast<milliseconds>(now() - estampa).count();
			cout << "Se estuvo " << duration_cast<milliseconds>(now() - estampa).count()
				 << "milisegundos ocioso." << endl;
		}
	}
	cout << "Ya no se van a ejecutar más procesos porque se cerró el server." << endl;
	pthread_exit(NULL);
}
void *algoritmoSJF(void *)
{
	ociosidad.lock();
	ociosidad.lock();
	while (corriendo.load())
	{
		Proceso p;
		bool flag = false;
		pthread_mutex_lock(&readyLock);
		if (ready.size() > 0)
		{
			int menor = 500;
			Proceso p;
			int eliminar;

			for (int i = 0; i < ready.size(); i++)
			{
				if (ready.at(i).burst < menor)
				{
					menor = ready.at(i).burst;
					eliminar = i;
				}
			}
			p = ready.at(eliminar);
			ready.erase(ready.begin() + eliminar);
			flag = true;
		}
		pthread_mutex_unlock(&readyLock);
		if (flag)
		{
			int id = p.pid;
			cout << "Ejecutando el proceso con pid = " << p.pid << " por " << p.burst << " segundos.\n"
				 << endl;
			listPCB.at(id).wt = time(NULL) - listPCB.at(id).wt;
			sleep(p.burst);
			listPCB.at(id).tat = time(NULL) - listPCB.at(id).tat;
			listPCB.at(id).estado = 1;
		}
		else
		{
			cout << "El procesador está ocioso, iniciando cronómetro de tiempo ocioso." << endl;
			const auto estampa = now();
			ociosidad.lock();
			tiempoOcioso += duration_cast<milliseconds>(now() - estampa).count();
			cout << "Se estuvo " << duration_cast<milliseconds>(now() - estampa).count()
				 << "milisegundos ocioso." << endl;
		}
	}
	cout << "Ya no se van a ejecutar más procesos porque se cerró el server." << endl;
	pthread_exit(NULL);
}
void *algoritmoHPF(void *)
{
	ociosidad.lock();
	ociosidad.lock();
	while (corriendo.load())
	{
		Proceso p;
		bool flag = false;
		pthread_mutex_lock(&readyLock);
		if (ready.size() > 0)
		{
			p = ready.at(0);
			cout << p.burst << ',' << p.prioridad << endl;
			for (int i = 1; i < ready.size(); i++)
				if (ready.at(i).prioridad <= p.prioridad)
					p = ready.at(i);
			for (int i = 0; i < ready.size(); i++)
				if (ready.at(i).pid == p.pid)
				{
					ready.erase(ready.begin() + i);
					break;
				}
			flag = true;
		}
		pthread_mutex_unlock(&readyLock);
		if (flag)
		{
			int id = p.pid;
			cout << "Ejecutando el proceso con pid = " << p.pid << " por " << p.burst << " segundos.\n"
				 << endl;
			listPCB.at(id).wt = time(NULL) - listPCB.at(id).wt;
			sleep(p.burst);
			listPCB.at(id).tat = time(NULL) - listPCB.at(id).tat;
			listPCB.at(id).estado = 1;
		}
		else
		{
			cout << "El procesador está ocioso, iniciando cronómetro de tiempo ocioso." << endl;
			const auto estampa = now();
			ociosidad.lock();
			tiempoOcioso += duration_cast<milliseconds>(now() - estampa).count();
			cout << "Se estuvo " << duration_cast<milliseconds>(now() - estampa).count()
				 << "milisegundos ocioso." << endl;
		}
	}
	cout << "Ya no se van a ejecutar más procesos porque se cerró el server." << endl;
	pthread_exit(NULL);
}
void *algoritmoRoundRobin(void *num)
{
	int q = *((int *)num);
	ociosidad.lock();
	ociosidad.lock();
	while (corriendo.load())
	{
		Proceso p;
		bool flag = false, flag2 = false;
		pthread_mutex_lock(&readyLock);
		if (ready.size() > 0)
		{
			p = ready.at(0);
			ready.erase(ready.begin());
			p.burst -= q;
			if (p.burst > 0)
				ready.push_back(p);
			else if (p.burst < 0)
			{
				p.burst += q;
				flag2 = true;
			}

			flag = true;
		}
		pthread_mutex_unlock(&readyLock);
		if (flag)
		{
			int id = p.pid;
			int burst = flag2 ? p.burst : q;
			cout << "Ejecutando el proceso con pid = "
				 << p.pid << " por " << burst << " segundos." << endl;
			if (p.burst <= q)
			{
				listPCB.at(id).wt = time(NULL) - listPCB.at(id).wt;
				sleep(burst);
				listPCB.at(id).tat = time(NULL) - listPCB.at(id).tat;
				listPCB.at(id).estado = 1;
			}
			else
			{
				listPCB.at(id).wt += burst;
				sleep(burst);
				ready.at(ready.size() - 1).burst -= burst;
				listPCB.at(id)
					.estado = 0;
			}
		}
		else
		{
			cout << "El procesador está ocioso, iniciando cronómetro de tiempo ocioso." << endl;
			const auto estampa = now();
			ociosidad.lock();
			tiempoOcioso += duration_cast<milliseconds>(now() - estampa).count();
			cout << "Se estuvo " << duration_cast<milliseconds>(now() - estampa).count()
				 << "milisegundos ocioso." << endl;
		}
	}
	cout << "Ya no se van a ejecutar más procesos porque se cerró el server." << endl;
	pthread_exit(NULL);
}
void *server(void *)
{
	int serverSocketHandler = socket(AF_INET, SOCK_STREAM, 0);
	// creating a socket and assigning it to the socket handler
	if (serverSocketHandler < 0)
	{
		// socket methode return -1 if the creation was not successful
		cout << "Socket creation has failed.";
	}
	struct sockaddr_in serverAddr, clientAddr;
	// specifying address and type for the server to operate under
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int bindStatus = bind(serverSocketHandler, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
	if (bindStatus < 0)
	{
		cout << "Socket binding has failed" << endl;
	}
	// listen to the client while others are waiting in queue of size 5
	int listenStatus = listen(serverSocketHandler, 5);
	if (listenStatus < 0)
	{ // when queue is full listen fails
		cout << "Listener has failed" << endl;
	}
	cout << "\t\t...Waiting for connections... \n\n";
	int clientSocketHandler;
	socklen_t len = sizeof(clientAddr);
	int connection;
	if ((connection = accept(serverSocketHandler, (struct sockaddr *)&clientAddr, &len)) < 0)
	{
		cout << "Server didn't accept the request." << endl;
	}
	else
	{
		cout << "Server accepted the request. \n";
	}

	char buff[MAX];

	pthread_t threadRecibir;
	if (pthread_create(&threadRecibir, NULL, esperaMensaje, (void *)connection) != 0)
	{
		cout << "Error in thread join " << endl;
	}
	pthread_exit(NULL);
}
int main(int argc, char *argv[])
{
	int rc;
	pthread_t threadServer;
	rc = pthread_create(&threadServer, NULL, server, NULL);
	if (rc)
	{
		cout << "Error:unable to create thread," << rc << endl;
		exit(-1);
	}
	pthread_t procesador;
	if (std::string(argv[1]) == "SJF")
	{
		pthread_create(&procesador, NULL, algoritmoSJF, NULL);
	}
	else if (std::string(argv[1]) == "FIFO")
	{
		pthread_create(&procesador, NULL, algoritmoFIFO, NULL);
	}
	else if (std::string(argv[1]) == "HPF")
	{
		pthread_create(&procesador, NULL, algoritmoHPF, NULL);
		cout << "hpf";
	}
	else if (std::string(argv[1]) == "RR")
	{
		int q;
		if (argc == 3)
		{
			q = atoi(argv[2]);
		}
		else
		{
			q = 3;
		}
		pthread_create(&procesador, 0, algoritmoRoundRobin, &q);
	}
	else
	{
		cout << "Parametros de entrada incorrectos\n";
	}
	pthread_exit(NULL);
}
