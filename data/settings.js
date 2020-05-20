jQuery( document ).ready(function( $ ) {
  $(document).ready( updateForm );
});

// update the value of form elements
function updateForm() {
$.getJSON( "json", function() {
  //alert( "success" );
})
  .done(function(data) {
    $.each(data, function(key, value) {
      $( document.getElementById("settings-form").elements[key] ).val(value)
    });
    //alert( "done" );
  })
  .fail(function() {
    //alert( "error" );
  })
  .always(function() {
    //alert( "finished" );
  });
}
