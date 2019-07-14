#include <iostream>
#include <thread>
#include <mutex>
#include <list>
#include <vector>
#include <chrono>
#include <fstream>
#include <condition_variable>
using namespace std;

mutex outMutex, startGroupMutex, positionMutex;
vector<unsigned int> positionUsers(10);
vector<unsigned int> totalRequests({0, 0}); // requests per group
vector<unsigned int> waits({0, 0}); // waits dur group and due position

unsigned int startingGroup, startingGroupActiveUsers=0;
bool startingGroupDone = false;
condition_variable resume_condition, position_avaiable;





void workerFunction(unsigned int userNo, unsigned int groupNo,
                    unsigned int position,
                    unsigned int start,
                    unsigned int duration) {





    if (groupNo == startingGroup) {
    	startGroupMutex.lock();
    	startingGroupActiveUsers++;
    	startGroupMutex.unlock();
    }

    // Update starting group count if necessary
    this_thread::sleep_for(chrono::seconds(start));

	   // Output start information
    outMutex.lock();
    cout << "User " << userNo << " from Group " << groupNo << " arrives to the DBMS" << endl;
    outMutex.unlock();




    // check if we are in the starting group. Wait on the condition if necessary
    //startGroupMutex.lock();
    while (groupNo != startingGroup && !startingGroupDone) {
		outMutex.lock();
		cout << "User "<< userNo <<" is waiting due to its group" << endl;
		waits[0]++; // update waits due to group
		outMutex.unlock();
		unique_lock<mutex> lk(startGroupMutex);
		resume_condition.wait(lk);
    }
    //startGroupMutex.unlock();



    // Now check if we can access the position in the database, wait otherwise
	 bool positionReady=false;
	 while (!positionReady) {
	 	positionMutex.lock();
	 	if (positionUsers[position - 1]==0) {
	 		positionUsers[position - 1] = userNo;
	 		positionReady=true;
	 		positionMutex.unlock();
	 	} else {

	 		outMutex.lock();
	 		cout << "User " << userNo << " is waiting: position " << position << " of the database is being used by user " << positionUsers[position - 1] << endl;
			waits[1]++; // update waits due to position
			outMutex.unlock();
	 		positionMutex.unlock();
	 		unique_lock<mutex> lk(positionMutex);
	    	position_avaiable.wait(lk);

	 	}


	 }
	 // Report that we are using the position
	 outMutex.lock();
	 cout << "User " << userNo << " is accessing the position " << position << " of the database for "<< duration << "  second(s)" << endl;
	 outMutex.unlock();

    this_thread::sleep_for(chrono::seconds(duration));

    // Remove ourselves from the list of positions
	 positionMutex.lock();
	 positionUsers[position - 1] = 0;
	 positionMutex.unlock();
	 position_avaiable.notify_all();

    outMutex.lock();
    cout << "User " << userNo << " finished its execution" << endl;
    outMutex.unlock();


        // Update starting group count if necessary
    if (groupNo == startingGroup) {
    	startGroupMutex.lock();
    	startingGroupActiveUsers--;
    	if (startingGroupActiveUsers==0) {
    		startingGroupDone=true;
    		outMutex.lock();
    		cout << endl;
    		cout << "All users from Group " << startingGroup << " finished their execution" << endl;
    		cout << "The users from Group " << (3 - startingGroup) << " start their execution" << endl;
			cout << endl;
			outMutex.unlock();
    	}
    	startGroupMutex.unlock();
    	if (startingGroupDone) {
    		resume_condition.notify_all();
      }
    }
}



/* run this program using the console pauser or add your own getch, system("pause") or input loop */

int main(int argc, char** argv) {
	list<thread*> workers;


	//ifstream inf("input.txt");
	if (cin >> startingGroup) {
		unsigned int userNo = 0;
		unsigned int groupNo, position, start, duration;
		bool startingGroupSeen = false;

		// initialize userIDs in positions
		for (auto it=positionUsers.begin(); it != positionUsers.end(); it++) {
			*it=0; // no user is accessing
		}

		while (cin >> groupNo >> position >> start >> duration) {
			totalRequests[groupNo-1]++; // update request count for the group
	   	workers.push_back(new thread(workerFunction,
			                             ++userNo, groupNo,
												  position, start, duration));
			if (groupNo == startingGroup) {
				startingGroupSeen=true;
			}
		}

		// if the starting group was not even seen mark as completed
		startGroupMutex.lock();
		if (!startingGroupSeen) {
			startingGroupDone=true;
			outMutex.lock();
    		cout << endl;
    		cout << "All users from Group " << startingGroup << " finished their execution" << endl;
    		cout << "The users from Group " << (3 - startingGroup) << " start their execution" << endl;
			cout << endl;
			outMutex.unlock();
		}
		startGroupMutex.unlock();
		if (startingGroupDone) {
    		resume_condition.notify_all();
      }

   }
	//inf.close();

	// join all threads
	for (auto x: workers) {
		x->join();
		// catch completing threads from starting group here
		delete x;
	}

	// Print summary
	cout << endl << "Total Requests:" << endl;
	for (unsigned int i=0; i<totalRequests.size(); i++) {
		cout << "         Group " << (i + 1) << ": " << totalRequests[i] << endl;
	}
	cout << endl << "Requests that waited:" << endl;
	cout << "         Due to its group: " << waits[0] << endl;
	cout << "         Due to a locked position: " << waits[1] << endl;

	return 0;
}
