import 'package:flutter/material.dart';

class BuddysEncontradosPage extends StatefulWidget {
  final List<dynamic> encontrados;

  BuddysEncontradosPage({required this.encontrados});

  @override
  _BuddysEncontradosPageState createState() => _BuddysEncontradosPageState();
}

class _BuddysEncontradosPageState extends State<BuddysEncontradosPage> {
  String _search = "";
  bool _ordemAlfabetica = true;
  bool _ordemGosta = false;

  List<dynamic> get _filteredList {
    List<dynamic> lista = List<dynamic>.from(widget.encontrados);

    // Filtro por nome
    if (_search.isNotEmpty) {
      lista = lista
          .where((e) => (e['nome'] ?? "")
          .toString()
          .toLowerCase()
          .contains(_search.toLowerCase()))
          .toList();
    }

    // Ordenação por gosta
    if (_ordemGosta) {
      lista.sort((a, b) {
        int getGosta(dynamic e) {
          if (e['gosta'] == true) return 0; // Gosta primeiro
          if (e['gosta'] == false) return 1; // Não gosta depois
          return 2; // Indefinido por último
        }

        int cmp = getGosta(a).compareTo(getGosta(b));
        if (cmp != 0) return cmp;
        return (a['nome'] ?? "").toString().toLowerCase().compareTo((b['nome'] ?? "").toString().toLowerCase());
      });
    } else if (_ordemAlfabetica) {
      lista.sort((a, b) => (a['nome'] ?? "")
          .toString()
          .toLowerCase()
          .compareTo((b['nome'] ?? "").toString().toLowerCase()));
    } else {
      lista = lista.reversed.toList(); // Z-A
    }
    return lista;
  }

  @override
  Widget build(BuildContext context) {
    final bgColor = Color(0xFFFFF7ED);

    return Scaffold(
      backgroundColor: bgColor,
      appBar: AppBar(
        elevation: 0,
        title: Text("Buddys Encontrados", style: TextStyle(color: Colors.white)),
        backgroundColor: Colors.orange,
        iconTheme: IconThemeData(color: Colors.white), // Seta branca
      ),
      body: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          children: [
            // Busca + Filtro dentro de Card
            Card(
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(22)),
              color: Colors.orange.shade50,
              elevation: 3,
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 16),
                child: Row(
                  children: [
                    Expanded(
                      child: TextField(
                        decoration: InputDecoration(
                          labelText: "Buscar pelo nome",
                          border: OutlineInputBorder(borderRadius: BorderRadius.circular(16)),
                          prefixIcon: Icon(Icons.search, color: Colors.orange),
                          isDense: true,
                          contentPadding: EdgeInsets.symmetric(vertical: 0, horizontal: 10),
                        ),
                        onChanged: (value) => setState(() => _search = value),
                      ),
                    ),
                    SizedBox(width: 10),
                    PopupMenuButton<String>(
                      icon: Icon(Icons.sort, color: Colors.orange),
                      onSelected: (value) {
                        setState(() {
                          if (value == "AZ") {
                            _ordemAlfabetica = true;
                            _ordemGosta = false;
                          } else if (value == "ZA") {
                            _ordemAlfabetica = false;
                            _ordemGosta = false;
                          } else if (value == "GOSTA") {
                            _ordemGosta = true;
                          }
                        });
                      },
                      itemBuilder: (context) => [
                        CheckedPopupMenuItem(
                          value: "AZ",
                          checked: _ordemAlfabetica && !_ordemGosta,
                          child: Text("Ordem alfabética (A-Z)"),
                        ),
                        CheckedPopupMenuItem(
                          value: "ZA",
                          checked: !_ordemAlfabetica && !_ordemGosta,
                          child: Text("Ordem alfabética (Z-A)"),
                        ),
                        CheckedPopupMenuItem(
                          value: "GOSTA",
                          checked: _ordemGosta,
                          child: Text("Gosta > Não gosta > Indefinido"),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            SizedBox(height: 20),
            // Lista em card arredondado e limpo
            Expanded(
              child: _filteredList.isEmpty
                  ? Center(
                child: Text(
                  "Nenhum Buddy encontrado.",
                  style: TextStyle(fontSize: 18),
                ),
              )
                  : ListView.separated(
                itemCount: _filteredList.length,
                separatorBuilder: (context, idx) => SizedBox(height: 12),
                itemBuilder: (context, i) {
                  final encontrado = _filteredList[i];
                  final nome = encontrado["nome"] ?? "";
                  String status;
                  if (encontrado.containsKey("gosta")) {
                    if (encontrado["gosta"] == true) {
                      status = "Gosta 👍";
                    } else {
                      status = "Não gosta 👎";
                    }
                  } else {
                    status = "Indefinido ❓";
                  }
                  return Card(
                    shape: RoundedRectangleBorder(
                      borderRadius: BorderRadius.circular(20),
                    ),
                    elevation: 2,
                    color: Colors.white,
                    child: ListTile(
                      contentPadding: EdgeInsets.symmetric(horizontal: 18, vertical: 8),
                      leading: CircleAvatar(
                        backgroundColor: Colors.orange.shade100,
                        child: Icon(Icons.face, color: Colors.orange[800]),
                      ),
                      title: Text(
                        nome,
                        style: TextStyle(
                          fontWeight: FontWeight.bold,
                          color: Colors.orange[900],
                          fontSize: 18,
                        ),
                      ),
                      trailing: Text(
                        status,
                        style: TextStyle(
                          fontWeight: FontWeight.w500,
                          fontSize: 16,
                          color: encontrado["gosta"] == true
                              ? Colors.green
                              : encontrado["gosta"] == false
                              ? Colors.red
                              : Colors.grey,
                        ),
                      ),
                    ),
                  );
                },
              ),
            ),
          ],
        ),
      ),
    );
  }
}
