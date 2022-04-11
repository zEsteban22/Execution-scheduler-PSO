#include <fstream>
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <vector>
#include <atomic>
#include <sstream>
#include <chrono>
#include <mutex>

using namespace std;
using chrono::duration_cast;
using chrono::milliseconds;

#define MAX 500
#define port 5200

struct Proceso
{
	int burst;
	int prioridad;
	int pid;
};

vector<Proceso> ready;
pthread_mutex_t readyLock;
mutex ociosidad;
atomic<bool> corriendo(true);
atomic<int> pid(0);
atomic<long> tiempoOcioso(0);
void *esperaMensaje(void *con)
{
	int connection = *((int *)(&con));
	char buff[MAX];
	while (true)
	{
		int rMsgSize;
		if ((rMsgSize = recv(connection, buff, MAX, 0)) > 0)
		{
			cout << "Client : " << buff << endl;
			if (buff[0] == 'b' && buff[1] == 'y' && buff[2] == 'e')
			{
				cout << "Server : Bye " << endl;
				cout << "\nConnection ended.\n";
				stringstream ss;
				ss << "Estado de la cola de ready!" << endl;
				ss << "Largo = " << ready.size() << endl;
				ss << "Elementos:" << endl;
				Proceso p;
				for (int i = 0; i < ready.size(); i++)
				{
					p = ready.at(i);
					ss << p.burst << ',' << p.prioridad << endl;
				}
				ss << "Y estuvo " << tiempoOcioso << " milisegundos haciendo nada." << endl;
				int l = ss.str().length();
				for (int i = 0; i < l; i++)
					buff[i] = ss.get();
				send(connection, buff, l + 1, 0);
				corriendo = false;
				break;
			}
			else
			{
				string linea = buff;
				pthread_mutex_lock(&readyLock);
				ready.push_back({atoi(linea.substr(0, linea.find(',')).c_str()), atoi(linea.substr(linea.find(','), linea.length()).c_str()), pid++});
				if (ready.size() == 1)
					ociosidad.unlock();
				pthread_mutex_unlock(&readyLock);
				linea = "pid = " + to_string(pid) + ".";
				int l = linea.length();
				for (int i = 0; i < l; i++)
					buff[i] = linea.at(i);
				send(connection, buff, l + 1, 0);
			}
		}
	}
	pthread_exit(NULL);
}

void *algoritmoFIFO(void *)
{
	cout << "A partir de ahora se ejecutarán los procesos en orden FIFO." << endl;
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
			cout << "Voy a ejecutar el proceso con pid = " << p.pid << " por " << p.burst << " segundos." << endl;
			sleep(p.burst);
		}
		else
		{
			const auto estampa = chrono::system_clock::now();
			cout << "Estoy ocioso." << endl;
			ociosidad.lock();
			cout << "Llegó alguien y me desperté." << endl;
			tiempoOcioso += duration_cast<milliseconds>(chrono::system_clock::now() - estampa).count();
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
		cout << "Listner has failed" << endl;
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
	while (true)
	{

		// cout << "Server : ";
		char input[MAX];
		string s;
		s = "";
		getline(cin, s);
		int n = s.size();
		for (int i = 0; i < n; i++)
		{
			input[i] = s[i];
		}

		input[n] = '\0';

		send(connection, input, strlen(input) + 1, 0);
	}
	pthread_exit(NULL);
}
int main()
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
	pthread_create(&procesador, NULL, algoritmoFIFO, NULL);
	pthread_exit(NULL);
}