#include<iostream>
#include"TcpSocket.hpp"


using namespace std;

int main(int argc, char* argv[])
{
   	if(argc != 3)
    {   
        cerr << "正确输入方式: ./tcp_cli.cc ip port\n" << endl;
        return -1; 
    } 
  
	string srv_ip = argv[1];
	uint16_t srv_port = stoi(argv[2]);
 
	TcpSocket socket;
	//创建套接字
    CheckSafe(socket.Socket());
	//申请连接服务器
	CheckSafe(socket.Connect(srv_ip, srv_port));

	while(1)
	{
		string data;
		cout << "cli send message: ";
		getline(cin, data);
		
		if(data == "quit")
		{
			break;
		}

		//发送数据
		CheckSafe(socket.SendNoBlock(data));
		data.clear();
	}

    //关闭套接字
    socket.Close();

    return 0;
}
