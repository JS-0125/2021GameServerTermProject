#include<vector>
#include<fstream>
#include<iostream>
#include<string>

using namespace std;

vector<vector<bool>> terrain;
vector<vector<bool>> terrainTest;

int worldSize = 2000;
int main()
{
	// make data
	for (int i = 0; i < worldSize; ++i) {
		vector<bool> tmp;
		tmp.reserve(worldSize);
		for (int n = 0; n < worldSize; ++n) {
			switch (rand()%6)
			{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
				tmp.push_back(1);
				break;
			case 5:
				tmp.push_back(0);
				break;
			}
		}
		terrain.emplace_back(tmp);
	}

	// save data
	ofstream os;
	os.open("terrainData.txt");
	for (int i = 0; i < worldSize; ++i) {
		copy(terrain[i].begin(), terrain[i].end(), std::ostreambuf_iterator<char>(os));
	}
	cout << "저장 완료" << endl;

	// read data
	ifstream is("terrainData.txt");
	terrainTest.resize(worldSize);

	for (int i = 0; i < worldSize; ++i) {
		terrainTest[i].reserve(worldSize);
		for (int n = 0; n < worldSize; ++n) {
			terrainTest[i].push_back(is.get());
		}
	}

}