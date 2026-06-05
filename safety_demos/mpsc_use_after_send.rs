use std::sync::mpsc;

fn main() {
    let (tx, rx) = mpsc::channel::<String>();
    let order_id = String::from("order-1001");

    tx.send(order_id).unwrap();

    println!("sent order: {order_id}");
    println!("received order: {}", rx.recv().unwrap());
}

