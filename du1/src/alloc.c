#include "wrapper.h"

/* Kod funkcii my_init, my_alloc a my_free nahradte vlastnym. Nepouzivajte ziadne
 * globalne ani staticke premenne; jedina globalna pamat je dostupna pomocou
 * mread/mwrite/msize, ktorych popis najdete vo wrapper.h */

/* Ukazkovy kod zvladne naraz iba jedinu alokaciu. V 0-tom bajte pamate si
 * pamata, ci je pamat od 1 dalej volna alebo obsadena. 
 *
 * V pripade, ze je volna, volanie my_alloc skonci uspesne a vrati zaciatok
 * alokovanej RAM; my_free pri volnej pamati zlyha.
 *
 * Ak uz nejaka alokacia prebehla a v 0-tom bajte je nenulova hodnota. Nie je
 * mozne spravit dalsiu alokaciu, takze my_alloc musi zlyhat. my_free naopak
 * zbehnut moze a uvolni pamat.
 */


/**
 * Inicializacia pamate
 *
 * Zavola sa, v stave, ked sa zacina s prazdnou pamatou, ktora je inicializovana
 * na 0.
 */
void my_init(void) {
	unsigned int s = msize();
	if (s < 9) return;

	//pouzivam 4B tail pre nieco podobne ako nextfit pointer
	mwrite(s-4, (0 >> 0) & 0xFF);
	mwrite(s-3, (0 >> 8) & 0xFF);
	mwrite(s-2, (0 >> 16) & 0xFF);
	mwrite(s-1, (0 >> 24) & 0xFF);

	s -= 5; //header
	s -= 4; //tail
	//pociatocny segment, hlavicka na zaciatku 5B, size 4B(int) + free flag
	mwrite(0, (s >> 0) & 0xFF);
	mwrite(1, (s >> 8) & 0xFF);
	mwrite(2, (s >> 16) & 0xFF);
	mwrite(3, (s >> 24) & 0xFF);

	//free flag = true
	mwrite(4, 1);

	

	return;
}

/**
 * Poziadavka na alokaciu 'size' pamate. 
 *
 * Ak sa pamat podari alokovat, navratova hodnota je adresou prveho bajtu
 * alokovaneho priestoru v RAM. Pokial pamat uz nie je mozne alokovat, funkcia
 * vracia FAIL.
 */

int read_int_4B(unsigned int addr){
	int num = 0;
	//TODO prepisat ak bude tam bug
	num += mread(addr);
	num += (mread(addr+1) << 8);
	num += (mread(addr+2) << 16);
	num += (mread(addr+3) << 24);

	return num;
}

int my_alloc(unsigned int size) {

	//najdeme prvy segment s free flag true, ak sa tam zmesti tak mozme alokovat pamat
	//First fit (FF)
	// + optimalizacia nieco podobne ako Next fit, prehladavame iba od adresy ulozene v tail, nie od 0
	int limit  = msize() -4;
	unsigned int addr = read_int_4B(msize()-4); //ulozene v tail tail
	while (addr + 5 <= limit)
	{
		int block_size = read_int_4B(addr);

		if(mread(addr + 4) == 1 && block_size >= size){
			//prerozdelime na 2 segmenty
			if (block_size >= size + 5 + 1){
				unsigned int next = addr + 5 + size;
				int free_size = block_size - size - 5;
				mwrite(next, (free_size >> 0) & 0xFF);
				mwrite(next +1, (free_size >> 8) & 0xFF);
				mwrite(next +2, (free_size >> 16) & 0xFF);
				mwrite(next +3, (free_size >> 24) & 0xFF);

				mwrite(next+4, 1); //free flag

				//prepiseme velkost obsadeneho blocku na size, nastavime flag na obsadene
				mwrite(addr, (size >> 0) & 0xFF);
				mwrite(addr +1, (size >> 8) & 0xFF);
				mwrite(addr +2, (size >> 16) & 0xFF);
				mwrite(addr +3, (size >> 24) & 0xFF);

				mwrite(addr +4, 0);
			}
			else {
				mwrite(addr +4, 0);
			}
			
			//do tail zapisem posledne upravenu adresu, pri dalsom vyhladavani nepojdem od 0, ale od posledneho alokovaneho.
			//staci nastavit na 0 vtedy, ked sa uvolni pamat, lebo volny blok pred aktualnym pointerom by mohol vzniknut iba uvolnovanim
			unsigned int next_ptr = addr + 5 + read_int_4B(addr);
			if (next_ptr + 5 > limit) next_ptr = 0; //safety

			mwrite(limit, (next_ptr >> 0) & 0xFF);
			mwrite(limit +1, (next_ptr >> 8) & 0xFF);
			mwrite(limit +2, (next_ptr >> 16) & 0xFF);
			mwrite(limit +3, (next_ptr >> 24) & 0xFF);
			
			return addr + 5;
		}
		else {
			addr = addr + 5 + read_int_4B(addr); //precitam velkost segmentu
		}
	}
	

	return FAIL;
}

/**
 * Poziadavka na uvolnenie alokovanej pamate na adrese 'addr'.
 *
 * Ak bola pamat zacinajuca na adrese 'addr' alokovana, my_free ju uvolni a
 * vrati OK. Ak je adresa 'addr' chybna (nezacina na nej ziadna alokovana
 * pamat), my_free vracia FAIL.
 */

int my_free(unsigned int addr) {

	//pozrieme hlavicky, podla size vieme skok na dalsi header, ak sme na hlavicke adresy addr,
	//tak skusime si zlucit so susednymi free blockmi

	//ak nenajdem hlavicku, tak je neplatna adresa
	int limit = msize() - 4;

	unsigned int prev_head = -1;
	unsigned int curr_addr = 0;

	while(curr_addr <= limit-5)
	{
		int curr_size = read_int_4B(curr_addr);
		int curr_free = mread(curr_addr +4);

		if(curr_addr + 5 == addr && curr_free == 0){
			//current bude free
			mwrite(curr_addr +4, 1);

			//skusime zlucit next a current (najprv toto, lebo hlavicka bude na adrese curr)
			unsigned int next_head = curr_addr + 5 + curr_size;
			if(next_head +5 <= limit){
				int next_size = read_int_4B(next_head);
				int next_free = mread(next_head +4);

				if(next_free == 1){
					int size_sum = curr_size + 5 + next_size;

					mwrite(curr_addr, (size_sum >> 0) & 0xFF);
					mwrite(curr_addr +1, (size_sum >> 8) & 0xFF);
					mwrite(curr_addr +2, (size_sum >> 16) & 0xFF);
					mwrite(curr_addr +3, (size_sum >> 24) & 0xFF);

					curr_size = size_sum;
				}
			}

			if(prev_head != -1){
				int prev_size = read_int_4B(prev_head);
				int prev_free = mread(prev_head +4);

				if(prev_free == 1){
					int size_sum2 = prev_size + 5 + curr_size;

					mwrite(prev_head, (size_sum2 >> 0) & 0xFF);
					mwrite(prev_head +1, (size_sum2 >> 8) & 0xFF);
					mwrite(prev_head +2, (size_sum2 >> 16) & 0xFF);
					mwrite(prev_head +3, (size_sum2 >> 24) & 0xFF);
				}
			}

			//nieco podobne ako Next fit, pri uvolnovani nastavim pointer adresy na 0, aby sme nasli uvolneny blok pri alokovani
			mwrite(limit, (0 >> 0) & 0xFF);
			mwrite(limit +1, (0 >> 8) & 0xFF);
			mwrite(limit +2, (0 >> 16) & 0xFF);
			mwrite(limit +3, (0 >> 24) & 0xFF);
			return OK;			
		}
		else{
			prev_head = curr_addr;
			curr_addr += 5 + curr_size;
		}
	}

	return FAIL;
}
