#include <iostream>
#include <cstdlib>
#include <deque>
#include <memory>
#include <list>
#include <set>
#include <utility>
#include <boost/asio.hpp>
#include "message.hpp"
using boost::asio::ip::tcp;
using namespace std;


typedef deque<message> MessageQueue;
class partipicant{
public:
    virtual ~partipicant() {}
    virtual void deliver (const message& MessageItem) = 0;
};

typedef shared_ptr<partipicant> PartipicantPointer;

class room{
public:
    void join(PartipicantPointer partipicant){
        partipicants.insert(partipicant);
        for(auto messageItem : MessageRecents){
            partipicant->deliver(messageItem);
        }
    }

    void deliver(const message& messageItem){
          MessageRecents.push_back(messageItem);
          while(MessageRecents.size() > max){
              MessageRecents.pop_front();
          }
          for(auto partipicant : partipicants){
              partipicant->deliver(messageItem);
          }
    }

    void leave(PartipicantPointer partipicant){
         partipicants.erase(partipicant);
    }

private:
    set<PartipicantPointer> partipicants;
    enum {max = 20};
    MessageQueue MessageRecents;

};

class session : public partipicant, public enable_shared_from_this<session>{

public:
     session(tcp::socket socket,room& room) : socket(move(socket)), room_(room){}


    void start(){
         room_.join(shared_from_this());
         readHeader();
     }

     void deliver(const message& messageItem){
          bool write_in_progress = !Messages.empty();
          Messages.push_back(messageItem);
          if(!write_in_progress){
              write();
          }
     }

private:

    void readHeader(){
         auto self(shared_from_this());
         boost::asio::async_read(socket,boost::asio::buffer(messageItem.data(),message::headerLength)
                                 ,[this,self](boost::system::error_code ec, size_t){
             if(!ec && messageItem.decodeHeader()){
                 readBody();
             }
             else{
                 room_.leave(shared_from_this());
             }
         });
     }

     void readBody(){
         auto self(shared_from_this());
         boost::asio::async_read(socket,boost::asio::buffer(messageItem.body(),messageItem.bodyLength())
                                 ,[this,self](boost::system::error_code ec, size_t){
                 if(!ec){
                     room_.deliver(messageItem);
                     readHeader();
                 }
                 else{
                     room_.leave(shared_from_this());
                 }
         });
     }



   void write(){
         auto self (shared_from_this());
         boost::asio::async_write(socket,boost::asio::buffer(Messages.front().data(),Messages.front().length()),
                [this,self](boost::system::error_code ec, size_t){
             if(!ec){
                 Messages.pop_front();
                 if(!Messages.empty()){
                     write();
                 }
             } else {
                 room_.leave(shared_from_this());
             }
         });
     }



    tcp::socket socket;
    room& room_;
    MessageQueue Messages;
    message messageItem;

};

class server{
public:
    server(boost::asio::io_context &context, tcp::endpoint& endpoint) : acceptor(context,endpoint){
        do_accept();
    }

private:

    void do_accept(){
        acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket){
              if(!ec){
                  make_shared<session>(move(socket),room_)->start();
              }
              do_accept();
        });

    }

    tcp::acceptor acceptor;
    room room_;

};

int main(int argc, char* argv[]) {
    try {
        if(argc < 2){
            cerr << "Usage: server <port> [<port> ...]\n";
            return 1;
        }
        boost::asio::io_context context;
        list<server> servers;

       for(int i = 1; i < argc; ++i){
           tcp::endpoint endpoint(tcp::v4(),atoi(argv[i]));
           servers.emplace_back(context,endpoint);
       }
       context.run();

    }

    catch(exception &e) {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
