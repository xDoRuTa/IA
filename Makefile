
texfile = Entrega2
bibfile = Referencias.bib
CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra
TARGET = solver
SRC = codigo.cpp

documento: $(texfile).tex $(bibfile) #para compilar el documento, CORRER 2 VECES!!, se hace en este orden para que se actualicen las referencias cruzadas y la bibliografía
	latex $(texfile).tex
	bibtex $(texfile).aux
	bibtex $(texfile).aux
	latex $(texfile).tex
	dvipdfmx $(texfile).dvi

clean: #para limpiar la basurilla del latex
	rm -f *~
	rm -f $(texfile).aux
	rm -f $(texfile).bbl
	rm -f $(texfile).blg
	rm -f $(texfile).dvi
	rm -f $(texfile).log
	rm -f $(texfile).out
	rm -f $(texfile).spl
	rm -f $(texfile).pdf
	rm -f $(texfile).tex.backup
	rm -f $(bibfile).backup

# ESTO ES PA EL CODIGITO
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET) #para correr el programa compilado
	./$(TARGET)

clean_code: #para limpiar la basurilla del codigo compilado y los outputs
	rm -f $(TARGET)
	rm -rf outputs   
