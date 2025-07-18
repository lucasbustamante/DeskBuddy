import 'package:deskbuddy/deskBuddyHomePage2.dart';
import 'package:flutter/material.dart';
import 'package:flutter_phoenix/flutter_phoenix.dart'; // <-- adicione isto

void main() => runApp(
  Phoenix(
    child: DeskBuddyApp(),
  ),
);

class DeskBuddyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'DeskBuddy BLE',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(primarySwatch: Colors.blue),
      home: DeskBuddyHomePage2(),
    );
  }
}
