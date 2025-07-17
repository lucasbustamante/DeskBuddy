import 'package:deskbuddy/deskBuddyHomePage.dart';
import 'package:flutter/material.dart';


void main() => runApp(DeskBuddyApp());

class DeskBuddyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'DeskBuddy BLE',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(primarySwatch: Colors.blue),
      home: DeskBuddyHomePage(),
    );
  }
}